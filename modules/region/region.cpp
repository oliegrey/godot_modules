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
	for (int i{ 0 }; i < m_secondary_regions.size(); ++i) {
		m_secondary_weights.set(i, m_secondary_regions[i]->spawn_weight);
	}

	m_primary_weights.resize(m_primary_regions.size());
	for (int i{ 0 }; i < m_primary_regions.size(); ++i) {
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

int Region::get_weight_sum_bounded(
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
	const int weights_sum
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
			gpos.y -= 1;
		} else if (dir == Direction::DOWN) {
			gpos.y += size.y;
		} else if (dir == Direction::LEFT) {
			gpos.x -= 1;
		} else if (dir == Direction::RIGHT) {
			gpos.x += size.x;
		}

		dir_to_free_edge_gpos[dir].push_back(gpos);
	}
}

void Region::generate_zone(
	Ref<RandomNumberGenerator> rng,
	Ref<PCG> pcg,
	const int level,
	const int max_secondary_count
) {
	Ref<BitGrid2D> occ{ pcg->generative_occupancy };

	// get a random weighted primary region
	const int bnd{ m_primary_weights.size() };
	const int primary_i{ rand_weighted_bound(rng, m_primary_weights, bnd, m_primary_weight_sum) };
	Ref<Region> primary{ m_primary_regions[primary_i] };

	// find a free area that will fit the region, starting the search at a random offset
	int rand_cell_i{ rng->randi_range(0, m_seg_cell_count) };
	const Vector2i p_g_size{ primary->g_size_inclusive };
	const int p_cell_i{ occ->find_area_in_state(p_g_size, rand_cell_i, rand_cell_i - 1) };
	auto p_gpos{ Vector2i(p_cell_i % m_seg_g_size.x, p_cell_i / m_seg_g_size.x) };

	// place dug tiles in the primaries cells
	const PackedInt32Array LAYER_OFFSETS{ 0 };
	const PackedInt32Array TILE_INDEXES{ 0 };
	pcg->add_tiles_rect(LAYER_OFFSETS, TILE_INDEXES, p_gpos, p_g_size);

	// how many secondaries we want
	int target_secondary_count{ rng->randi_range(0, max_secondary_count) };

	// free grid position look up based on direction requirement; dir -> [free edge gpos, ...]
	std::array<LocalVector<Vector2i>, Direction::DIRECTION_MAX> dir_to_free_edge_gpos{};
	for (int dir{ 0 }; dir < Direction::DIRECTION_MAX; ++dir) {
		dir_to_free_edge_gpos[dir].reserve(target_secondary_count * 4 + 4);
	}

	// add primaries free edge grid positions
	add_free_edge_gpos(primary, p_gpos, dir_to_free_edge_gpos);

	// get threshold from ordered secondary regions to determine cutoff
	int threshold_i{ 0 };
	for (; threshold_i < m_secondary_regions.size(); ++threshold_i) {
		if (m_secondary_regions[threshold_i]->threshold > level) { break; }
	}

	int secondary_weight_sum{ get_weight_sum_bounded(m_secondary_weights, threshold_i) };

	// amount of times to reiterate over failed region additions after adding other regions
	const int MAX_ATTEMPTS{ 3 };
	LocalVector<Ref<Region>> region_rejects{};
	region_rejects.reserve(m_secondary_regions.size());

	// a tree to find grid positions that have needed free area sizes related to directions
	// dir * max size in cells + size in cells -> grid position
	std::array<Vector2i, FLAT_TREE_SIZE> dir_size_to_gpos;
	std::array<uint64_t, Direction::DIRECTION_MAX> dir_size_to_gpos_occ{};


	for (int i{ 0 }; i < target_secondary_count; ++i) {
		Ref<Region> candidate{
			rand_weighted_bound(rng, m_secondary_weights, threshold_i, secondary_weight_sum)
		};

		get_next_set_bit(dir_size_to_gpos_occ[]);
		// we need to track which we have already searched so we dont search again right?
	}

	for (int attempt{ 0 }; attempt < MAX_ATTEMPTS; ++attempt) {
		for (Ref<Region> region : region_rejects) {

		}
	}



	//// attachment direction [0 - 3]
	//// -> quad size flattened [(0 * 0), ... (max * 0), ... (max * max)]
	//// -> region
	//// to find a region that fits after a failed test with free space
	//// order quad size from smallest to largest so we can find the next lowest
	//// region vector ordered by min world segment, can iterate to find the max
	//inline static std::array<LocalVector<LocalVector<Ref<Region>>>, 4> m_region_tree;

	//// attachment direction (already placed regions perspective) [0 - 3] -> region
	//// to get a random region to test in a free direction
	//// region vector ordered by min world segment, can iterate to find the max
	//inline static std::array<LocalVector<Ref<Region>>, 4> m_dir_to_region;

	//// starting regions to build off of
	//inline static LocalVector<Ref<Region>> m_primary_regions;
}
