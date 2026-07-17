#include "region.h"
#include "modules/pcg/pcg.h"
#include "modules/bit_grid_2d/bit_grid_2d.h"
#include "core/math/random_number_generator.h"

#include "scene/gui/label.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

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

	ClassDB::bind_static_method(
		"Region",
		D_METHOD("generate_zone", "rng", "pcg", "w_seg", "max_secondary_count"),
		&Region::generate_zone
	);

	ClassDB::bind_method(
		D_METHOD("get_name"), &Region::get_name
	);
	ClassDB::bind_method(
		D_METHOD("get_slot"), &Region::get_slot
	);
	ClassDB::bind_method(
		D_METHOD("get_g_size"), &Region::get_g_size
	);
	ClassDB::bind_method(
		D_METHOD("get_blocked_sides"), &Region::get_blocked_sides
	);
	ClassDB::bind_method(
		D_METHOD("get_joining_sides"), &Region::get_joining_sides
	);
	ClassDB::bind_method(
		D_METHOD("get_spawn_weight"), &Region::get_spawn_weight
	);
	ClassDB::bind_method(
		D_METHOD("get_threshold"), &Region::get_threshold
	);
	ClassDB::bind_method(
		D_METHOD("get_g_size_inclusive"), &Region::get_g_size_inclusive
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::STRING, "name"),
		"", "get_name"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT, "slot", PROPERTY_HINT_ENUM,
			"Primary,Secondary"
		),
		"", "get_slot"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::VECTOR2I, "g_size"),
		"", "get_g_size"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::PACKED_INT32_ARRAY, "blocked_sides"),
		"", "get_blocked_sides"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::PACKED_INT32_ARRAY, "joining_sides"),
		"", "get_joining_sides"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::FLOAT, "spawn_weight"),
		"", "get_spawn_weight"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::INT, "threshold"),
		"", "get_threshold"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::VECTOR2I, "g_size_inclusive"),
		"", "get_g_size_inclusive"
	);
	
	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(UP);
	BIND_ENUM_CONSTANT(DOWN);
	BIND_ENUM_CONSTANT(LEFT);
	BIND_ENUM_CONSTANT(RIGHT);
	BIND_ENUM_CONSTANT(DIRECTION_MAX);

	BIND_ENUM_CONSTANT(PRIMARY);
	BIND_ENUM_CONSTANT(SECONDARY);

	BIND_ENUM_CONSTANT(FILL);
	BIND_ENUM_CONSTANT(RANDOM);

	BIND_ENUM_CONSTANT(DIRT);
	BIND_ENUM_CONSTANT(STONE);
	BIND_ENUM_CONSTANT(RANDOM);
	BIND_ENUM_CONSTANT(ANY);
	BIND_ENUM_CONSTANT(ANY_STONE);
}

String Region::get_name() const {
	return name;
}

Region::Slot Region::get_slot() const {
	return slot;
}

Vector2i Region::get_g_size() const {
	return g_size;
}

PackedInt32Array Region::get_blocked_sides() const {
	return blocked_sides;
}

PackedInt32Array Region::get_joining_sides() const {
	return joining_sides;
}

float Region::get_spawn_weight() const {
	return spawn_weight;
}

int Region::get_threshold() const {
	return threshold;
}

Vector2i Region::get_g_size_inclusive() const {
	return g_size_inclusive;
}

void Region::initialize(Vector2i seg_g_size, bool debug) {
	for (int dir{ 0 }; dir < m_region_tree.size(); ++dir) {
		m_region_tree[dir].resize(MAX_CELL_COUNT);
	}
	m_seg_g_size = seg_g_size;
	m_seg_cell_count = seg_g_size.x * seg_g_size.y;
	is_debug = debug;
	init_dominance_mask();
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

	region->g_size_inclusive = _g_size;
	for (int dir : region->blocked_sides) {
		if (dir == Direction::UP || dir == Direction::DOWN) {
			region->g_size_inclusive.y += 1;
		} else if (dir == Direction::LEFT || dir == Direction::RIGHT) {
			region->g_size_inclusive.x += 1;
		}
	}

	ERR_FAIL_COND_V_MSG(region->g_size_inclusive.x > 8, Ref<Region>(), "g_size_inclusive.x > 8");
	ERR_FAIL_COND_V_MSG(region->g_size_inclusive.y > 8, Ref<Region>(), "g_size_inclusive.y > 8");

	region->m_cell_count = region->g_size_inclusive.y * region->g_size_inclusive.x;

	if (_slot == Slot::PRIMARY) {
		m_primary_regions.push_back(region);
		return region;
	} else {
		m_secondary_regions.push_back(region);
	}

	//for (int i{ 0 }; i < _joining_sides.size(); ++i) {
	//	Direction direction{ static_cast<Direction>(_joining_sides[i]) };
	//	Direction inv_direction{ invert_direction(direction) };
	//	m_dir_to_region[inv_direction].push_back(region);

	//	const int tree_i{ inv_direction * MAX_CELL_COUNT + (region->m_cell_count - 1) };
	//	CRASH_BAD_INDEX(tree_i, FLAT_TREE_SIZE);
	//	m_region_tree[tree_i].push_back(region);
	//	m_region_tree_occ[inv_direction] |= 1ull << (region->m_cell_count - 1);
	//}

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


	for (int dir{ 0 }; dir < Direction::DIRECTION_MAX; ++dir) {
		std::sort(
			m_dir_to_region[dir].ptr(),
			m_dir_to_region[dir].ptr() + m_dir_to_region[dir].size(),
			[](const Ref<Region> &a, const Ref<Region> &b) {
				return a->threshold < b->threshold;
			}
		);
	}

	//for (int cell_i{ 0 }; cell_i < MAX_CELL_COUNT; ++cell_i) {
	//	int cell_i{ 0 };

	//	

	//	const int i{ dir * MAX_CELL_COUNT + cell_i };

	//	std::sort(
	//		m_region_tree[i].ptr(),
	//		m_region_tree[i].ptr() + m_region_tree[i].size(),
	//		[](const Ref<Region> &a, const Ref<Region> &b) {
	//			return a->threshold < b->threshold;
	//		}
	//	);

	//	++cell_i;
	//}

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

void Region::init_dominance_mask() {
	constexpr int size{ 8 };
	for (int tx{ 1 }; tx <= size; ++tx) {
		for (int ty{ 1 }; ty <= size; ++ty) {
			uint64_t mask{ 0 };
			for (int x{ tx }; x <= size; ++x) {
				for (int y{ ty }; y <= size; ++y) {
					mask |= 1ull << get_size_i(Vector2i(x, y));
				}
			}

			const int ix{ tx - 1 };
			const int iy{ ty - 1 };

			if (ix < 0 || ix >= size || iy < 0 || iy >= size) {
				continue; // unreachable given loop bounds, but satisfies /analyze
			}

			dominance_mask[ix][iy] = mask;
		}
	}
}

int Region::get_size_or_larger_i(uint64_t bitmap, const Vector2i size) {
	uint64_t mask{ dominance_mask[size.x - 1][size.y - 1] };
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
	Ref<Region> region, Vector2i gpos, DirEdge& dir_to_free_edge_gpos
) {
	Vector2i size{ region->g_size };

	for (int dir{ 0 }; dir < Direction::DIRECTION_MAX; ++dir) {
		if (region->blocked_sides.has(dir)) {
			continue;
		}

		Vector2i edge_gpos{ gpos };

		if (dir == Direction::UP) {
			edge_gpos.y -= 1;
			if (region->blocked_sides.has(Direction::LEFT)) {
				edge_gpos.x += 1;
			}

		} else if (dir == Direction::DOWN) {
			edge_gpos.y += size.y;
			if (region->blocked_sides.has(Direction::LEFT)) {
				edge_gpos.x += 1;
			}

		} else if (dir == Direction::LEFT) {
			edge_gpos.x -= 1;
			if (region->blocked_sides.has(Direction::UP)) {
				edge_gpos.y += 1;
			}

		} else if (dir == Direction::RIGHT) {
			edge_gpos.x += size.x;
			if (region->blocked_sides.has(Direction::UP)) {
				edge_gpos.y += 1;
			}
		}

		if (
			edge_gpos.x < 0 || edge_gpos.y < 0 ||
			edge_gpos.x >= m_seg_g_size.x || edge_gpos.y >= m_seg_g_size.y
		) {
			continue;
		}

		dir_to_free_edge_gpos[dir].push_back(Edge{ edge_gpos, size });
	}
}

// we need to come back to add the stone edges
// then we need to add the fillings
void Region::generate_zone(
	Ref<RandomNumberGenerator> rng,
	Ref<PCG> pcg,
	const int w_seg,
	const int max_secondary_count
) {
	Ref<BitGrid2D> gen_occupancy{ pcg->generative_occupancy };

	// how many secondaries we want
	int target_secondary_count{ max_secondary_count }; //rng->randi_range(max_secondary_count / 4, max_secondary_count)


	// free grid position look up based on direction requirement; dir -> [free edge gpos, g_size, ...]
	DirEdge dir_to_free_edge_gpos{};
	for (int dir{ 0 }; dir < Direction::DIRECTION_MAX; ++dir) {
		dir_to_free_edge_gpos[dir].reserve(target_secondary_count * 4 + 4);
	}

	// get a random weighted primary region
	const int64_t bnd{ m_primary_weights.size() };
	const int primary_i{ rand_weighted_bound(rng, m_primary_weights, bnd, m_primary_weight_sum) };
	ERR_FAIL_COND(primary_i == -1);
	Ref<Region> p_region{ m_primary_regions[primary_i] };

	// find a free area that will fit the region, starting the search at a random offset
	int rand_cell_i{ rng->randi_range(0, m_seg_cell_count - 1) };
	const Vector2i p_g_size{ p_region->g_size_inclusive };
	const int p_cell_i{ gen_occupancy->find_area_in_state(p_g_size, rand_cell_i, rand_cell_i - 1) };
	ERR_FAIL_COND(p_cell_i == -1);
	auto p_gpos{ Vector2i(p_cell_i % m_seg_g_size.x, p_cell_i / m_seg_g_size.x) };

	// place dug tiles in the primaries cells
	add_region(p_region, pcg, p_gpos, dir_to_free_edge_gpos, w_seg);

	// get threshold from ordered secondary regions to determine cutoff
	uint64_t threshold_i{ 0 };
	for (; threshold_i < m_secondary_regions.size(); ++threshold_i) {
		if (m_secondary_regions[threshold_i]->threshold > w_seg) { break; }
	}
	ERR_FAIL_COND_MSG(threshold_i <= 0, "no secondary regions found below threshold");

	float secondary_weight_sum{ get_weight_sum_bounded(m_secondary_weights, threshold_i) };
	ERR_FAIL_COND_MSG(secondary_weight_sum <= 0.0, "secondary weight sum is <= 0.0");

	LocalVector<Ref<Region>> s_region_rejects{};
	s_region_rejects.reserve(m_secondary_regions.size());

	// a tree to find grid positions that have needed free area sizes related to directions
	// dir * max size in cells + size in cells -> grid position
	std::array<PackedVector2Array, FLAT_TREE_SIZE> dir_size_to_gpos;
	std::array<uint64_t, Direction::DIRECTION_MAX> dir_size_occ{};

	for (int i{ 0 }; i < target_secondary_count; ++i) {
		const int secondary_i{
			rand_weighted_bound(rng, m_secondary_weights, threshold_i, secondary_weight_sum)
		};

		Ref<Region> s_region{ m_secondary_regions[secondary_i] };

		bool is_placed{
			try_place_s_region(
				rng,
				s_region,
				dir_size_occ,
				dir_size_to_gpos,
				pcg,
				dir_to_free_edge_gpos,
				gen_occupancy,
				w_seg
			)
		};

		if (!is_placed) {
			s_region_rejects.push_back(s_region);
		}
	}

	//WARN_PRINT(vformat("s_region_rejects size is %d", s_region_rejects.size()));

	const int MAX_ATTEMPTS{ 4 };

	RegionVector temp_rejects{};

	for (int attempt{ 0 }; attempt < MAX_ATTEMPTS; ++attempt) {

		if (attempt > 0) {
			s_region_rejects = temp_rejects;
		}

		temp_rejects.resize(0);

		for (Ref<Region> s_region: s_region_rejects) {

			bool success {
				try_place_s_region(
					rng,
					s_region,
					dir_size_occ,
					dir_size_to_gpos,
					pcg,
					dir_to_free_edge_gpos,
					gen_occupancy,
					w_seg
				)
			};
			//WARN_PRINT(vformat("retry attempt %d to place region resulted in %s", attempt, success));

			if (!success) {
				temp_rejects.push_back(s_region);
			}
		}

		if (temp_rejects.size() == s_region_rejects.size()) {
			break; // impossible to get any more to place, no point retrying
		}
	}
}

int Region::get_size_i(Vector2i size) {
	return (size.x - 1) + (size.y - 1) * MAX_G_SIZE.x;
}

bool Region::try_place_s_region(
	Ref<RandomNumberGenerator> rng,
	Ref<Region> s_region,
	std::array<uint64_t, Direction::DIRECTION_MAX> &dir_size_occ,
	std::array<PackedVector2Array, FLAT_TREE_SIZE> &dir_size_to_gpos,
	Ref<PCG> pcg,
	DirEdge &dir_to_free_edge_gpos,
	Ref<BitGrid2D> gen_occupancy,
	int w_seg
) {
	//warn_print(vformat("attempting to place %s", s_region->name));

	const int64_t side_count{ s_region->joining_sides.size() };
	int start_dir_i{ rng->randi_range(0, side_count - 1) };

	for (int64_t dir_offset{ 0 }; dir_offset < side_count; ++dir_offset) {

		int dir_i{ s_region->joining_sides[(start_dir_i + dir_offset) % side_count] };

		Direction dir{ static_cast<Direction>(dir_i) };
		Direction req_dir{ invert_direction(dir) };

		const Vector2i g_size_inc{ s_region->g_size_inclusive };
		const Vector2i g_size{ s_region->g_size };

		LocalVector<Edge> &free_edge_gpos{ dir_to_free_edge_gpos[req_dir] };

		// look for it in the tree of already searched and catalogued areas
		const int req_dir_offset{ req_dir * MAX_CELL_COUNT };

		int size_i_fit{ };
		PackedVector2Array *sized_gpos{ };
		int64_t idx{ 0 };

		while (true) {

			if (idx <= 0) {
				size_i_fit = get_size_or_larger_i(dir_size_occ[req_dir], g_size_inc);
				if (size_i_fit == -1) {
					break;
				}
				sized_gpos = &dir_size_to_gpos[req_dir_offset + size_i_fit];
				idx = sized_gpos->size();
			}

			--idx;

			//warn_print(vformat("found size fit in already scanned for %s", s_region->name));
			Vector2i gpos{ (*sized_gpos)[idx] };
			Vector2i found_size{ (size_i_fit % MAX_G_SIZE.x) + 1, (size_i_fit / MAX_G_SIZE.x) + 1 };

			if (dir == Direction::DOWN) {
				gpos.y -= found_size.y - 1;
			} else if (dir == Direction::RIGHT) {
				gpos.x -= found_size.x - 1;
			}

			PackedVector2Array org_size{
				gen_occupancy->find_anchored_unset_areas_in_bounds(
					gpos,
					found_size,
					static_cast<BitGrid2D::Direction>(dir),
					g_size_inc
				)
			};

			// edge is no longer empty for whatever reason
			if (org_size.size() < 2) {
				// edge cant be used so remove it
				sized_gpos->set(idx, (*sized_gpos)[sized_gpos->size() - 1]);
				sized_gpos->resize(sized_gpos->size() - 1);
				if (sized_gpos->size() == 0) {
					dir_size_occ[req_dir] &= ~(1ull << size_i_fit);
				}

				//warn_print(vformat("edge full on already searched %d", org_size.size()));
				continue;
			}

			// there is enough room
			if (org_size.size() == 2 && org_size[1] == g_size_inc) {
				Vector2i dir_offset_gpos{ org_size[0] };
				if (req_dir == Direction::UP) {
					dir_offset_gpos.y -= g_size_inc.y - 1;
				} else if (req_dir == Direction::LEFT) {
					dir_offset_gpos.x -= g_size_inc.x - 1;
				}

				// is this even segment position, or is it in the search space in some way
				add_region(s_region, pcg, dir_offset_gpos, dir_to_free_edge_gpos, w_seg);

				// edge filled so remove it
				sized_gpos->set(idx, (*sized_gpos)[sized_gpos->size() - 1]);
				sized_gpos->resize(sized_gpos->size() - 1);
				if (sized_gpos->size() == 0) {
					dir_size_occ[req_dir] &= ~(1ull << size_i_fit);
				}

				// edge is being used so remove it
				//warn_print(vformat("placed from already searched at %s", org_size));
				return true;
			}

			// edge doesnt have this size anymore so remove it
			sized_gpos->set(idx, (*sized_gpos)[sized_gpos->size() - 1]);
			sized_gpos->resize(sized_gpos->size() - 1);
			if (sized_gpos->size() == 0) {
				dir_size_occ[req_dir] &= ~(1ull << size_i_fit);
			}
			

			// there are now smaller areas
			for (int i{ 0 }; i < org_size.size(); i += 2) {
				Vector2i found_origin{ org_size[i] };
				Vector2i found_size{ org_size[i + 1] };

				const int s_i{ get_size_i(found_size) };

				dir_size_to_gpos[req_dir_offset + s_i].push_back(found_origin);
				dir_size_occ[req_dir] |= 1ull << s_i;

				//warn_print(vformat("set bit from already searched: %s, origin: %s, size %s", s_i, found_origin, found_size));
			}
		}


		// otherwise search possible areas
		// iterate backwards so we can swap remove items from the end without issues
		//warn_print(vformat("testing joining side %s (req dir %s)", dir_i, req_dir));

		for (int64_t gpos_i{ static_cast<int64_t>(free_edge_gpos.size()) - 1 }; gpos_i >= 0 ; --gpos_i) {
			Vector2i gpos{ free_edge_gpos[gpos_i].gpos };
			Vector2i prev_g_size{ free_edge_gpos[gpos_i].size };

			//warn_print(vformat("trying free edge grid position %s", gpos));

			// set the search origin and size so if g_size is found it will always be
			// connected to the previous region while using the maximum search size
			Vector2i search_origin{ gpos };
			Vector2i search_size{ 8, 8 };
			Vector2i wanted_size{ g_size_inc };

			// different rules for 1 length if there are stone sides because it will never fit
			if (req_dir == Direction::UP) {
				search_origin.y -= 7;

				if (prev_g_size.x == 1 && s_region->blocked_sides.has(Direction::LEFT)) {
					search_size.x = 1;
					wanted_size = Vector2i(0, 0);
				}

				else {
					search_size.x = prev_g_size.x + g_size.x;
					if (!s_region->blocked_sides.has(Direction::RIGHT)) {
						search_size.x -= 1;
					}
				}
				
			} else if (req_dir == Direction::DOWN) {
				if (prev_g_size.x == 1 && s_region->blocked_sides.has(Direction::RIGHT)) {
					search_size.x = 1;
					wanted_size = Vector2i(0, 0);
				}

				else {
					search_origin.x -= g_size.x; 
					search_size.x = prev_g_size.x + g_size.x;
					
					if (!s_region->blocked_sides.has(Direction::LEFT)) {
						search_origin.x += 1;
						search_size.x -= 1;
					}
				}

			} else if (req_dir == Direction::LEFT) {;
				search_origin.x -= 7;

				if (prev_g_size.y == 1 && s_region->blocked_sides.has(Direction::UP)) {
					search_size.y = 1;
					wanted_size = Vector2i(0, 0);
				}

				else {
					search_origin.y -= g_size.y;
					search_size.y = prev_g_size.y + g_size.y;
					
					if (!s_region->blocked_sides.has(Direction::UP)) {
						search_origin.y += 1;
						search_size.y -= 1;
					}
				}


			} else if (req_dir == Direction::RIGHT) {
				if (prev_g_size.y == 1 && s_region->blocked_sides.has(Direction::UP)) {
					search_size.y = 1;
					wanted_size = Vector2i(0, 0);
				}

				else {
					search_size.y = prev_g_size.x + g_size.x;
					if (!s_region->blocked_sides.has(Direction::DOWN)) {
						search_size.y -= 1;
					}
				}
			}

			//warn_print(vformat("search origin %s, search size %s", search_origin, search_size));

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
				// edge cant be used so remove it
				free_edge_gpos[gpos_i] = free_edge_gpos[free_edge_gpos.size() - 1];
				free_edge_gpos.resize(free_edge_gpos.size() - 1);

				//warn_print(vformat("edge full %d", org_size.size()));
				continue;
			}

			// there is enough room
			if (org_size.size() == 2 && org_size[1] == g_size_inc) {
				Vector2i dir_offset_gpos{ org_size[0] };
				if (dir == Direction::DOWN) {
					dir_offset_gpos.y -= g_size_inc.y - 1;
				} else if (dir == Direction::RIGHT) {
					dir_offset_gpos.x -= g_size_inc.x - 1;
				}

				free_edge_gpos[gpos_i] = free_edge_gpos[free_edge_gpos.size() - 1];
				free_edge_gpos.resize(free_edge_gpos.size() - 1);

				// is this even segment position, or is it in the search space in some way
				add_region(s_region, pcg, dir_offset_gpos, dir_to_free_edge_gpos, w_seg);

				// edge is being used so remove it
				//warn_print(vformat("placed at %s", org_size));

				return true;
			}

			// we did not find enough room
			for (int i{ 0 }; i < org_size.size(); i += 2) {
				Vector2i found_origin{ org_size[i] };
				Vector2i found_size{ org_size[i + 1] };

				const int s_i{ get_size_i(found_size) };
				dir_size_to_gpos[req_dir_offset + s_i].push_back(found_origin);
				dir_size_occ[req_dir] |= 1ull << s_i;

				//warn_print(vformat("set bit: %s, origin: %s, size %s", s_i, found_origin, found_size));
			}
		}
	}

	return false;
}

void Region::add_region(
	Ref<Region> region,
	Ref<PCG> pcg,
	Vector2i gpos,
	DirEdge &dir_to_free_edge_gpos,
	int w_seg
) {
	const PackedInt32Array LAYER_OFFSETS{ 0 };
	const PackedInt32Array TILE_INDEXES{ 0 };
	pcg->add_tiles_rect(LAYER_OFFSETS, TILE_INDEXES, gpos, region->g_size_inclusive, true);
	add_free_edge_gpos(region, gpos, dir_to_free_edge_gpos);
	
	if (is_debug) {
		WARN_PRINT(vformat("added region %s at %s", region->name, gpos));
		String a{ "" };
		for (int dir{ 0 }; dir < Direction::DIRECTION_MAX; ++dir) {
			a += "[";
			for (Edge edge : dir_to_free_edge_gpos[dir]) {
				a += vformat("(gpos%s, size%s), ", edge.gpos, edge.size);
			}
			a += "], ";
		}
		WARN_PRINT(vformat("added edge positions for w_seg %s:\n%s", w_seg, a));
		debug_region(gpos, region, w_seg);
	}
}


void Region::debug_region(Vector2i gpos, Ref<Region> region, int w_seg) {
	const Vector2i size{ region->g_size_inclusive };
	SceneTree *tree{ SceneTree::get_singleton() };
	Node *main{ tree->get_root()->get_node(NodePath("Main")) };

	for (int y{ 0 }; y < size.y; ++y) {
		for (int x{ 0 }; x < size.x; ++x) {
			Label *label{ memnew(Label) };

			label->add_theme_font_size_override("font_size", 12);
			label->set_text(region->name);
			label->set_autowrap_mode(TextServer::AUTOWRAP_ARBITRARY);
			label->set_custom_minimum_size(Vector2(50, 50));

			Vector2 pos;
			pos.x = (gpos.x + x) * 50;
			pos.y = (gpos.y + y + w_seg * m_seg_g_size.y) * 50;
			label->set_position(pos);

			label->set_z_index(999);

			main->add_child(label);
		}
	}
}

