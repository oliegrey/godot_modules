#include "region.h"
#include "modules/pcg/pcg.h"
#include "modules/bit_grid_2d/bit_grid_2d.h"
#include "core/math/random_number_generator.h"

#include <algorithm>

#if defined(_MSC_VER)
#include <intrin.h>
static inline int ctz32(uint32_t x) {
	unsigned long index;
	_BitScanForward(&index, x);
	return static_cast<int>(index);
}
static inline int ctz64(uint64_t x) {
	unsigned long index;
	_BitScanForward64(&index, x);
	return static_cast<int>(index);
}
#else
static inline int ctz32(uint32_t x) {
	return __builtin_ctz(x);
}
static inline int ctz64(uint64_t x) {
	return __builtin_ctzll(x);
}
#endif

void Region::_bind_methods() {
	ClassDB::bind_static_method(
		"Region", D_METHOD("initialize", "max_g_size"), &Region::initialize
	);
	ClassDB::bind_static_method(
		"Region",
		D_METHOD(
			"create",
			"_name",
			"_slot",
			"_size",
			"_blocked_sides",
			"_joining_sides",
			"_spawn_weight",
			"_threshold"
		),
		&Region::create
	);
	ClassDB::bind_static_method("Region", D_METHOD("finalize"), &Region::finalize);

	ClassDB::bind_static_method("Region", D_METHOD("generate_zone", "rng", "pcg"), &Region::generate_zone);

	ClassDB::bind_static_method(
		"Region",
		D_METHOD("get_weight_sum_bounded", "p_weights", "exl_upper_bound"),
		&Region::get_weight_sum_bounded
	);
	ClassDB::bind_static_method(
		"Region",
		D_METHOD("rand_weighted_bound", "rng", "p_weights", "exl_upper_bound", "weights_sum"),
		&Region::rand_weighted_bound
	);

	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(UP);
	BIND_ENUM_CONSTANT(DOWN);
	BIND_ENUM_CONSTANT(LEFT);
	BIND_ENUM_CONSTANT(RIGHT);
	BIND_ENUM_CONSTANT(DIRECTION_MAX);
}

void Region::initialize(Vector2i seg_g_size) {
	for (int dir{ 0 }; dir < m_region_tree.size(); ++dir) {
		m_region_tree[dir].resize(MAX_CELL_COUNT);
	}
	m_seg_g_size = seg_g_size;
	m_seg_cell_count = seg_g_size.x * seg_g_size.y;
}

Ref<Region> Region::create(
	String _name,
	Slot _slot,
	Vector2i _g_size,
	PackedInt32Array _blocked_sides,
	PackedInt32Array _joining_sides,
	int _spawn_weight,
	int _threshold
) {
	ERR_FAIL_COND_V_MSG(
		m_region_tree[0].size() == 0, Ref<Region>(), "Region is not intialized"
	);

	Ref<Region> region;
	region.instantiate();
	region->name          = _name;
	region->slot          = _slot;
	region->g_size        = _g_size;
	region->blocked_sides = _blocked_sides;
	region->joining_sides = _joining_sides;
	region->spawn_weight  = _spawn_weight;
	region->threshold     = _threshold;

	if (_slot == Slot::PRIMARY) {
		m_primary_regions.push_back(region);
		return region;
	} else {
		m_secondary_regions.push_back(region);
	}

	region->m_cell_count = _g_size.y * _g_size.x;

	ERR_FAIL_COND_V_MSG(
		region->m_cell_count < MAX_CELL_COUNT,
		Ref<Region>(),
		"size of region is larger than max size provided in intialization"
	);

	for (int i{ 0 }; i < _joining_sides.size(); ++i) {
		Direction direction{ static_cast<Direction>(_joining_sides[i]) };
		Direction inv_direction{ invert_direction(direction) };
		m_dir_to_region[inv_direction].push_back(region);

		CRASH_BAD_INDEX(inv_direction * region->m_cell_count, MAX_CELL_COUNT);
		m_region_tree[inv_direction * region->m_cell_count].push_back(region);
		m_region_tree_occ[inv_direction] &= 1ull << (region->m_cell_count - 1);
	}

	return region;
}

void Region::finalize() {
	std::sort(
		m_primary_regions.ptr(),
		m_primary_regions.ptr() + m_primary_regions.size(),
		[](const Ref<Region> &a, const Ref<Region> &b) {
			return a->threshold < b->threshold;
		}
	);

	std::sort(
		m_secondary_regions.ptr(),
		m_secondary_regions.ptr() + m_secondary_regions.size(),
		[](const Ref<Region> &a, const Ref<Region> &b) {
			return a->threshold < b->threshold;
		}
	);

	for (int dir{ 0 }; dir < m_region_tree.size(); ++dir) {
		std::sort(
			m_dir_to_region[dir].ptr(),
			m_dir_to_region[dir].ptr() + m_region_tree[dir].size(),
			[](const Ref<Region> &a, const Ref<Region> &b) {
				return a->threshold < b->threshold;
			}
		);

		int cell_i{ 0 };
		const uint64_t bitmap{ m_region_tree_occ[dir] };

		while ((cell_i = get_next_set_bit(bitmap, cell_i)) != -1) {

			const int i{ dir * MAX_CELL_COUNT + cell_i };
			std::sort(
				m_region_tree[i].ptr(),
				m_region_tree[i].ptr() + m_region_tree[i].size(),
				[](const Ref<Region> &a, const Ref<Region> &b) {
					return a->threshold < b->threshold;
				}
			);

			++cell_i;
		}
	}

	m_secondary_weights.resize(m_secondary_regions.size());
	for (uint64_t i{ 0 }; i < m_secondary_regions.size(); ++i) {
		m_secondary_weights.set(i, m_secondary_regions[i]->spawn_weight);
	}

	m_primary_weights.resize(m_primary_regions.size());
	for (uint64_t i{ 0 }; i < m_primary_regions.size(); ++i) {
		m_primary_weights.set(i, m_primary_regions[i]->spawn_weight);
	}
	m_primary_weight_sum = get_weight_sum_bounded(m_primary_weights, m_primary_weights.size());
}

int Region::get_next_set_bit(uint64_t bitmap, const int start_i) {
	const uint64_t mask{ ~0ull << start_i };
	const uint64_t masked_bitmap{ bitmap & mask };
	if (masked_bitmap == 0) { return -1; }
	return ctz64(masked_bitmap);
}

float Region::get_weight_sum_bounded(
	const PackedFloat32Array &p_weights, const int exl_upper_bound
) {
	const float *weights = p_weights.ptr();
	float weights_sum = 0.0;
	for (int64_t i = 0; i < exl_upper_bound; ++i) {
		weights_sum += weights[i];
	}
	return weights_sum;
}

int Region::rand_weighted_bound(
	const Ref<RandomNumberGenerator> rng,
	const PackedFloat32Array &p_weights,
	const int exl_upper_bound,
	const float weights_sum
) {
	const float *weights = p_weights.ptr();
	float remaining_distance = rng->randf() * weights_sum;
	for (int64_t i{ 0 }; i < exl_upper_bound; ++i) {
		remaining_distance -= weights[i];
		if (remaining_distance < 0) {
			return i;
		}
	}
	return exl_upper_bound - 1;
}

void Region::add_free_edge_gpos(
	Ref<Region> region, Vector2i gpos, DirVectors& dir_to_free_edge_gpos
) {
	Vector2i size{ region->g_size_inclusive };
	for (int dir : region->joining_sides) {
		Vector2i edge_gpos{ gpos };

		if (dir == Direction::UP) {
			edge_gpos.y -= 1;
		} else if (dir == Direction::DOWN) {
			edge_gpos.y += size.y;
		} else if (dir == Direction::LEFT) {
			edge_gpos.x -= 1;
		} else if (dir == Direction::RIGHT) {
			edge_gpos.x += size.x;
		}

		dir_to_free_edge_gpos[dir].push_back(edge_gpos);
		dir_to_free_edge_gpos[dir].push_back(size);
	}
}

// we need to come back to add the stone edges
// then we need to add the fillings
void Region::generate_zone(
	Ref<RandomNumberGenerator> rng,
	Ref<PCG> pcg,
	const int level,
	const int max_secondary_count
) {
	Ref<BitGrid2D> gen_occupancy{ pcg->generative_occupancy };

	// how many secondaries we want
	int target_secondary_count{ rng->randi_range(0, max_secondary_count) };

	// free grid position look up based on direction requirement; dir -> [free edge gpos, g_size, ...]
	DirVectors dir_to_free_edge_gpos{};
	for (int dir{ 0 }; dir < Direction::DIRECTION_MAX; ++dir) {
		dir_to_free_edge_gpos[dir].reserve((target_secondary_count * 4 + 4) * 2);
	}

	// get a random weighted primary region
	const int64_t bnd{ m_primary_weights.size() };
	const int primary_i{ rand_weighted_bound(rng, m_primary_weights, bnd, m_primary_weight_sum) };
	Ref<Region> p_region{ m_primary_regions[primary_i] };

	// find a free area that will fit the region, starting the search at a random offset
	int rand_cell_i{ rng->randi_range(0, m_seg_cell_count - 1) };
	const Vector2i p_g_size{ p_region->g_size_inclusive };
	const int p_cell_i{ gen_occupancy->find_area_in_state(p_g_size, rand_cell_i, rand_cell_i - 1) };
	auto p_gpos{ Vector2i(p_cell_i % m_seg_g_size.x, p_cell_i / m_seg_g_size.x) };

	// place dug tiles in the primaries cells
	add_region(p_region, pcg, p_gpos, dir_to_free_edge_gpos);

	// get threshold from ordered secondary regions to determine cutoff
	uint64_t threshold_i{ 0 };
	for (; threshold_i < m_secondary_regions.size(); ++threshold_i) {
		if (m_secondary_regions[threshold_i]->threshold > level) { break; }
	}

	float secondary_weight_sum{ get_weight_sum_bounded(m_secondary_weights, threshold_i) };

	LocalVector<Ref<Region>> s_region_rejects{};
	s_region_rejects.reserve(m_secondary_regions.size());

	// a tree to find grid positions that have needed free area sizes related to directions
	// dir * max size in cells + size in cells -> grid position
	std::array<Vector2i, FLAT_TREE_SIZE> dir_size_to_gpos;
	std::array<uint64_t, Direction::DIRECTION_MAX> dir_size_occ{};

	for (int i{ 0 }; i < target_secondary_count; ++i) {
		const int secondary_i{
			rand_weighted_bound(rng, m_secondary_weights, threshold_i, secondary_weight_sum)
		};

		Ref<Region> s_region{ m_secondary_regions[secondary_i] };

		bool is_placed{ try_place_s_region(s_region, dir_size_occ, dir_size_to_gpos, pcg, dir_to_free_edge_gpos, gen_occupancy) };

		if (!is_placed) {
			s_region_rejects.push_back(s_region);
		}
	}

	const int MAX_ATTEMPTS{ 3 };

	RegionVector temp_rejects{};

	for (int attempt{ 0 }; attempt < MAX_ATTEMPTS; ++attempt) {

		if (attempt > 0) {
			s_region_rejects = temp_rejects;
		}

		temp_rejects.resize(0);

		for (Ref<Region> s_region: s_region_rejects) {

			bool success {
				try_place_s_region(
					s_region,
					dir_size_occ,
					dir_size_to_gpos,
					pcg,
					dir_to_free_edge_gpos,
					gen_occupancy
				)
			};
			if (!success) {
				temp_rejects.push_back(s_region);
			}
		}

		if (temp_rejects.size() == s_region_rejects.size()) {
			break; // impossible to get any more to place, no point retrying
		}
	}
}

bool Region::try_place_s_region(
	Ref<Region> s_region,
	std::array<uint64_t, Direction::DIRECTION_MAX> &dir_size_occ,
	std::array<Vector2i, FLAT_TREE_SIZE> &dir_size_to_gpos,
	Ref<PCG> pcg,
	DirVectors &dir_to_free_edge_gpos,
	Ref<BitGrid2D> gen_occupancy
) {
	for (int dir_i : s_region->joining_sides) {
		Direction dir{ static_cast<Direction>(dir_i) };
		const Vector2i g_size_inc{ s_region->g_size_inclusive };

		// look for it in the tree of already searched and catalogued areas
		const int dir_offset{ dir * MAX_CELL_COUNT };
		const int size_i{ get_next_set_bit(dir_size_occ[dir], 0) };
		if (size_i != -1) {
			Vector2i gpos{ dir_size_to_gpos[dir_offset + size_i] };
			add_region(s_region, pcg, gpos, dir_to_free_edge_gpos);
			return true;
		}

		// otherwise search possible areas
		// iterate backwards to make removing searched areas fast
		int64_t gpos_i{ static_cast<int64_t>(dir_to_free_edge_gpos[dir].size()) - 2 };

		if (gpos_i == -2) {
			continue;
		}

		for (; gpos_i >= 0; gpos_i -= 2) {
			Vector2i gpos{ dir_to_free_edge_gpos[dir][gpos_i] };
			Vector2i prev_g_size{ dir_to_free_edge_gpos[dir][gpos_i + 1] };

			// set the search origin and size so if g_size is found it will always be
			// connected to the previous region while using the maximum search size
			Vector2i search_origin{ gpos };
			Vector2i search_size{ 8, 8 };

			if (dir == Direction::UP) {
				search_origin.y -= 7;
				search_size.x = g_size_inc.x + prev_g_size.x / 2;
			} else if (dir == Direction::DOWN) {
				search_origin.y += 7;
				search_size.x = g_size_inc.x + prev_g_size.x / 2;
				search_origin.x -= search_size.x;
			} else if (dir == Direction::LEFT) {
				search_origin.x -= 7;
				search_size.y = g_size_inc.y + prev_g_size.y / 2;
			} else if (dir == Direction::RIGHT) {
				search_origin.x += 7;
				search_size.y = g_size_inc.y + prev_g_size.y / 2;
				search_origin.y -= search_size.y;
			}

			PackedVector2Array org_size{
				gen_occupancy->find_anchored_unset_areas_in_bounds(
					search_origin,
					search_size,
					static_cast<BitGrid2D::Direction>(dir),
					g_size_inc
				)
			};

			// edge is full
			if (org_size.size() < 2) {
				continue;
			}

			// there is enough room
			if (org_size[1] == g_size_inc) {
				add_region(s_region, pcg, gpos, dir_to_free_edge_gpos);
				dir_to_free_edge_gpos[dir].resize(gpos_i);
				return true;
			}

			// we did not find enough room
			for (int i{ 0 }; i < org_size.size(); i += 2) {
				Vector2i found_origin{ org_size[i] };
				Vector2i found_size{ org_size[i + 1] };

				if ( // skip result if the area is disconnected from the previous region
					(dir == Direction::UP && found_origin.x > gpos.x + prev_g_size.x - 1) ||
					(dir == Direction::DOWN && found_origin.x < gpos.x) ||
					(dir == Direction::LEFT && found_origin.y < gpos.y) ||
					(dir == Direction::RIGHT && found_origin.y > gpos.y + prev_g_size.y - 1)
				) {
					continue;
				}

				const int size_i{ found_size.x * found_size.y };
				dir_size_to_gpos[dir_offset + size_i] = found_origin;
				dir_size_occ[dir] |= 1ull << size_i;
			}
		}

		dir_to_free_edge_gpos[dir].resize(0); // should retain allocation size
	}

	return false;
}

void Region::add_region(
	Ref<Region> region, Ref<PCG> pcg, Vector2i gpos, DirVectors &dir_to_free_edge_gpos
) {
	const PackedInt32Array LAYER_OFFSETS{ 0 };
	const PackedInt32Array TILE_INDEXES{ 0 };
	pcg->add_tiles_rect(LAYER_OFFSETS, TILE_INDEXES, gpos, region->g_size_inclusive);
	// add primaries free edge grid positions
	add_free_edge_gpos(region, gpos, dir_to_free_edge_gpos);
}
