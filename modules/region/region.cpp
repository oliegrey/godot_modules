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
}

int Region::get_next_set_bit(uint64_t bitmap, const int start_i) {
	const uint64_t mask{ ~0ull << start_i };
	const uint64_t masked_bitmap{ bitmap & mask };
	if (masked_bitmap == 0) { return -1; }
	return ctz64(masked_bitmap);
}

//Ref<Region> search_tree

void Region::generate_zone(
	Ref<RandomNumberGenerator> rng,
	Ref<PCG> pcg,
	const int level,
	const int max_secondary_count
) {
	Ref<BitGrid2D> occ{ pcg->generative_occupancy };

	int primary_i{ rng->randi_range(0, m_primary_regions.size() - 1) };
	Ref<Region> primary{ m_primary_regions[primary_i] };

	int rand_cell_i{ rng->randi_range(0, m_seg_cell_count) };
	const Vector2i p_g_size{ primary->g_size_inclusive };
	const int p_cell_i{ occ->find_area_in_state(p_g_size, rand_cell_i, rand_cell_i - 1) };
	auto p_gpos{ Vector2i(p_cell_i % m_seg_g_size.x, p_cell_i / m_seg_g_size.x) };

	const PackedInt32Array LAYER_OFFSETS{ 0 };
	const PackedInt32Array TILE_INDEXES{ 0 };

	pcg->add_tiles_rect(LAYER_OFFSETS, TILE_INDEXES, p_gpos, p_g_size);

	std::array<int64_t, 4> size_gpos_occ{ 0 };
	std::array<std::array<Vector2i, 64>, 4> dir_size_gpos{};

	
	std::array<PackedFloat32Array, Direction::DIRECTION_MAX> dir_weights;

	// create lists of weights for the group of possible regions for each direction
	for (int dir{ 0 }; dir < Direction::DIRECTION_MAX; ++dir) {
		dir_weights[dir].resize(m_secondary_regions.size());
		RegionVector group{ m_dir_to_region[dir] };

		size_t j{ 0 };
		for (; j < group.size(); ++j) {
			Ref<Region> secondary{ group[j] };
			if (group[j]->threshold > level) {
				break;
			}
			dir_weights[dir].set(j, secondary->spawn_weight);
		}
		dir_weights[dir].resize(j);
	}

	int target_secondary_count{ rng->randi_range(0, max_secondary_count) };

	// IMPORTANT: we get a random direction from off of the primary and following
	// secondaries, because the other option is ass. this biases against higher weighted
	// options if they dont have more sides open... is there a good way to fix for this?
	// maybe divide the weighting by the amount of sides option so they appear more
	// on those sides when they are chosen?

	// 1. go through each direction in order, starting with a random offset
	// 2. pick a region based on weighted random
	// 3. check if that regions size or above already exists in the region tree for that direction
	// 4. for each edge available in that direction check if the area fits the region
	// if not then add it to the position tree, try again until exhausted, add region to waiting list
	// if it does then place it

	// dir * max_size + size -> position
	std::array<Vector2i, FLAT_TREE_SIZE> dir_size_to_pos;
	std::array<uint64_t, Direction::DIRECTION_MAX> dir_size_to_pos_occ{};

	for (int i{ 0 }; i < target_secondary_count; ++i) {
		const int rand_dir{ rng->randi_range(0, Direction::DIRECTION_MAX - 1) };
		const int64_t max_group_i{ dir_weights[rand_dir].size() };
		const int64_t rand_i{ rng->rand_weighted(dir_weights[rand_dir]) };
		
		for (int dir_offset{ 0 }; dir_offset < Direction::DIRECTION_MAX; ++dir_offset) {
			int dir{ (rand_dir + dir_offset) % Direction::DIRECTION_MAX };
			RegionVector dir_group{ m_dir_to_region[dir] };

			// max_group_i limits the sorted list based on threshold value
			for (int group_offset{ 0 }; group_offset < max_group_i; ++group_offset) {
				const int64_t group_i{ (rand_i + group_offset) % max_group_i };
				const Ref<Region> secondary{ dir_group[group_i] };

				const Vector2i secondary_size_inc{ secondary->g_size_inclusive };
				const int region_cell_count{ secondary_size_inc.x * secondary_size_inc.y };
				const int64_t occupancy{ dir_size_to_pos_occ[dir] };
				const int tree_dir_cell_i{ get_next_set_bit(occupancy, region_cell_count) };

				if (tree_dir_cell_i != -1) {
					Vector2i gpos{ dir_size_to_pos[dir * MAX_CELL_COUNT + tree_dir_cell_i] };
					// actually add it with the pcg
					continue; // go to add the next secondary
				}

				// attempt the current direction 
				
			}
		}

		

		
	}


	const int MAX_ATTEMPTS{ 1 };
	LocalVector<Ref<Region>> secondary_rejects{};

	// check already scanned edges hashmap and if not inside, go to the tree
	// start from random edge then wrap around the list, checking occupancy
		// everything that has occupancy of 1x1 or larger is added to a hashmap
		// everything that does not have occupancy is removed

	// if it cant be placed, add it to a list to try again at the end


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
