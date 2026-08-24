#include "zone.h"
#include "modules/pcg/pcg.h"
#include "modules/bit_grid_2d/bit_grid_2d.h"
#include "modules/tile/Tile.h"
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

void Zone::_bind_methods() {
	ClassDB::bind_static_method(
		"Zone", D_METHOD("create", "rng", "pcg", "max_secondary_count"), &Zone::create
	);
}

void Zone::init_dominance_mask() {
	constexpr int size{ 8 };
	for (int tx{ 1 }; tx <= size; ++tx) {
		for (int ty{ 1 }; ty <= size; ++ty) {
			uint64_t mask{ 0 };
			for (int x{ tx }; x <= size; ++x) {
				for (int y{ ty }; y <= size; ++y) {
					const int s_i{ get_size_i(Vector2i{ x, y }) };
					ERR_FAIL_COND(s_i == -1);
					mask |= 1ull << s_i;
				}
			}

			const int ix{ tx - 1 };
			const int iy{ ty - 1 };

			if (ix < 0 || ix >= size || iy < 0 || iy >= size) {
				continue; // unreachable given loop bounds, but satisfies /analyze
			}

			s_dominance_mask[ix][iy] = mask;
		}
	}
}

void Zone::initialize(Vector2i seg_g_size, bool is_debug) {
	s_seg_g_size = seg_g_size;
	s_seg_cell_count = seg_g_size.x * seg_g_size.y;
	s_is_debug = is_debug;
	init_dominance_mask();
}

Ref<Zone> Zone::create(
	Ref<RandomNumberGenerator> rng, Ref<PCG> pcg, int max_secondary_count, int w_seg
) {
	ERR_FAIL_COND_V_MSG(
		s_seg_g_size == Vector2i(0, 0), Ref<Zone>(),
		"initialize(...) must be called before creating any zones"
	);
	Ref<Zone> zone;
	zone.instantiate();
	zone->m_rng = rng;
	zone->m_pcg = pcg;
	zone->m_occ = m_pcg->generative_occupancy;
	zone->m_max_secondary_count = max_secondary_count;
	zone->m_w_seg = w_seg;
	for (int dir{ 0 }; dir < Direction::MAX; ++dir) {
		dir_to_free_edge_gpos[dir].reserve(max_secondary_count * 4 + 4);
	}
	zone->generate();
	return zone;
}

// we need to come back to add the stone edges
// then we need to add the fillings
void Zone::generate() {
	const int target_secondary_count{ m_max_secondary_count }; //rng->randi_range(m_max_secondary_count / 4, m_max_secondary_count)

	// get a random weighted primary region
	const int64_t bnd{ Region::primary_weights.size() };
	const int primary_i{ rand_weighted_bound(primary_weights, bnd, primary_weight_sum) };
	ERR_FAIL_COND(primary_i == -1);
	Ref<Region> p_region{ primary_regions[primary_i] };

	// get a random region size
	Vector2i length_addition{
		rng->randi_range(0, p_region->rand_length_addition.x),
		rng->randi_range(0, p_region->rand_length_addition.y)
	};
	Vector2i rand_g_size{ p_region->g_size + length_addition };
	Vector2i rand_g_size_inc{ p_region->g_size_inclusive + length_addition };

	ERR_FAIL_COND_MSG(
		rand_g_size_inc.x > 8 || rand_g_size_inc.y > 8,
		"region size + rand + blocked sides must be <= (8, 8)"
	);

	// find a free area that will fit the region, starting the search at a random offset
	int rand_cell_i{ rng->randi_range(0, m_seg_cell_count - 1) };
	const int p_cell_i{
		gen_occupancy->find_area_in_grid(rand_g_size_inc, rand_cell_i, rand_cell_i - 1)
	};
	ERR_FAIL_COND(p_cell_i == -1);
	auto p_gpos{ Vector2i(p_cell_i % m_seg_g_size.x, p_cell_i / m_seg_g_size.x) };

	p_region->add_region(rng, pcg, p_gpos, rand_g_size, dir_to_free_edge_gpos);
	if (is_debug) {
		debug_region(p_gpos, rand_g_size_inc, p_region, w_seg);
	}

	// get threshold from ordered secondary regions to determine cutoff
	uint64_t threshold_i{ 0 };
	for (; threshold_i < secondary_regions.size(); ++threshold_i) {
		if (secondary_regions[threshold_i]->threshold > w_seg) { break; }
	}
	ERR_FAIL_COND_MSG(threshold_i <= 0, "no secondary regions found below threshold");

	float secondary_weight_sum{ get_weight_sum_bounded(secondary_weights, threshold_i) };
	ERR_FAIL_COND_MSG(secondary_weight_sum <= 0.0, "secondary weight sum is <= 0.0");

	LocalVector<Ref<Region>> s_region_rejects{};
	s_region_rejects.reserve(secondary_regions.size());

	// a tree to find grid positions that have needed free area sizes related to directions
	// dir * max size in cells + size in cells -> grid position
	std::array<LocalVector<Vector2i>, FLAT_TREE_SIZE> dir_size_to_gpos;
	std::array<uint64_t, Direction::DIRECTION_MAX> dir_size_occ{};

	for (int i{ 0 }; i < target_secondary_count; ++i) {
		const int secondary_i{
			rand_weighted_bound(rng, secondary_weights, threshold_i, secondary_weight_sum)
		};

		Ref<Region> s_region{ secondary_regions[secondary_i] };

		bool is_placed{
			s_region->try_place_s_region(
				rng,
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


	//////////////////////////////

	const int MAX_ATTEMPTS{ 4 };

	RegionVector temp_rejects{};

	for (int attempt{ 0 }; attempt < MAX_ATTEMPTS; ++attempt) {

		if (attempt > 0) {
			s_region_rejects = temp_rejects;
		}

		temp_rejects.resize(0);

		for (Ref<Region> s_region: s_region_rejects) {

			bool success {
				s_region->try_place_s_region(
					rng,
					dir_size_occ,
					dir_size_to_gpos,
					pcg,
					dir_to_free_edge_gpos,
					gen_occupancy,
					w_seg
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

//////////////////////////////////////////////////////////////
///// end so far /////////////////////////////////////////////
//////////////////////////////////////////////////////////////

int Zone::rand_weighted_bound(
	const PackedFloat32Array &p_weights,
	const int exl_upper_bound,
	const float weights_sum
) {
	const float *weights = p_weights.ptr();
	float remaining_distance = m_rng->randf() * weights_sum;
	for (int64_t i{ 0 }; i < exl_upper_bound; ++i) {
		remaining_distance -= weights[i];
		if (remaining_distance < 0) {
			return i;
		}
	}
	return exl_upper_bound - 1;
}

int Zone::get_region_relative_size_i(Vector2i size) {
	const int size_i{ (size.x - 1) + (size.y - 1) * Region::MAX_G_SIZE_X };
	ERR_FAIL_INDEX_V(size_i, 64, -1);
	return size_i;
}

// returns whether the array is now empty
bool Zone::remove_edge(
	LocalVector<Vector2i> &free_gpos_arr,
	int edge_i,
	uint64_t &dir_occupancy,
	int size_cell_i
) {
	const int final_i{ free_gpos_arr.size() - 1 };

	if (edge_i != final_i) {
		const Vector2i temp{ free_gpos_arr[final_i] };
		free_gpos_arr[final_i] = free_gpos_arr[edge_i];
		free_gpos_arr[edge_i] = temp;
	}
	free_gpos_arr.resize(final_i);
	if (free_gpos_arr.size() == 0) {
		dir_occupancy &= ~(1ull << size_cell_i);
		return true;
	}
	return false;
}

void Zone::fill_blocked_sides(
	const LocalVector<Region::BlockedSide> &blocked_sides,
	const Rect2i &region_rect
) {
	const int side_count{ blocked_sides.size() };
	const int corner_count{ side_count * side_count * 0.25 };

	uint8_t corner_bitmap{ 0 };

	for (int i{ 0 }; i < side_count; ++i) {
		const Region::BlockedSide &blocked_side{ blocked_sides[i] };
		const int dir{ blocked_side.direction };

		Rect2i edge_rect{ region_rect };

		if (dir == Direction::UP) {
			edge_rect.position.y -= 1;
			edge_rect.size.y = 1;
			
		} else if (dir == Direction::DOWN) {
			edge_rect.position.y += region_rect.size.y;
			edge_rect.size.y = 1;

		} else if (dir == Direction::LEFT) {
			edge_rect.position.x -= 1;
			edge_rect.size.x = 1;

		} else if (dir == Direction::RIGHT) {
			edge_rect.position.x += region_rect.size.x;
			edge_rect.size.x = 1;
		}

		pcg->rand_fill_rect(rng, blocked_side.fill, blocked_side.tiles, edge_rect);
		corner_bitmap |= 1 << dir;
	}
	if ((corner_bitmap & 0b0101) == 0b0101) { // top left corner
		Vector2i corner_gpos{ region_rect.position + Vector2i{ -1, -1 } };
		fill_corner(rng, blocked_sides, corner_gpos);
	}
	if ((corner_bitmap & 0b0110) == 0b0110) { // bottom left corner
		Vector2i corner_gpos{ region_rect.position + Vector2i{ -1, region_rect.size.y } };
		fill_corner(rng, blocked_sides, corner_gpos);
	}
	if ((corner_bitmap & 0b1010) == 0b1010) { // bottom right corner
		Vector2i corner_gpos{ region_rect.position + region_rect.size };
		fill_corner(rng, blocked_sides, corner_gpos);
	}
	if ((corner_bitmap & 0b1001) == 0b1001) { // top right corner
		Vector2i corner_gpos{ region_rect.position + Vector2i{ region_rect.size.x, -1 } };
		fill_corner(rng, blocked_sides, corner_gpos);
	}
}

void Zone::fill_corner(
	Ref<RandomNumberGenerator> rng,
	const LocalVector<Region::BlockedSide> &blocked_sides,
	const Vector2i& corner_gpos
) {
	const int i{ rng->randi_range(0, 1) ? Direction::UP : Direction::LEFT };
	const LocalVector<Ref<Tile>> &tiles{ blocked_sides[i].tiles };
	m_pcg->rand_fill_rect(rng, PCG::Fill::PICK_ONE, tiles, { corner_gpos, { 1, 1 } }, true);
}

// consumes edge if it is not free to use, adds new smaller areas if possible
// removes the final free_gpos_arr element if not free
bool Zone::try_add_region_to_edge(
	Direction::E dir, Vector2i edge_gpos, Vector2i _inclusive_g_size
) {
	// check that it is free (fast if it is free and prevents overlapping regions for rechecks)
	BitGrid2D::Direction bit_dir{ static_cast<BitGrid2D::Direction>(dir) };
	LocalVector<Rect2i> org_size{
		m_occ->find_largest_anchored_areas_in_area(
			edge_gpos, _inclusive_g_size, bit_dir, m_rng, _inclusive_g_size
		)
	};

	bool is_free{ org_size[0].size == _inclusive_g_size };

	if (is_free) {
		Vector2i dir_offset_gpos{ org_size[0].position };

		// transform edge grid position to placement grid position
		const Direction::E req_dir{ Direction::invert(dir) };
		if (req_dir == Direction::UP) {
			dir_offset_gpos.y -= _inclusive_g_size.y - 1;
		} else if (req_dir == Direction::LEFT) {
			dir_offset_gpos.x -= _inclusive_g_size.x - 1;
		}

		add_region(m_rng, m_pcg, dir_offset_gpos, _inclusive_g_size, dir_to_free_edge_gpos);
		if (s_is_debug) {
			debug_region(dir_offset_gpos, _inclusive_g_size, this, m_w_seg);
		}
	}

	// in every case, the edge is consumed
	remove_edge(free_gpos_Arr, i, dir_occupancy, size_cell_i);

	// if more than one free size found add all from the beginning or after exact match
	if (org_size.size() > 1) {
		const int start_i{ static_cast<int>(is_free) };
		add_dir_size_to_gpos(
			dir_size_occ, dir_size_to_gpos,
			req_dir_offset, req_dir,
			org_size, start_i
		);
	}

	return is_free;
}

void Zone::add_free_edge_gpos(
	Vector2i internal_gpos, Vector2i rand_g_size, DirEdge& dir_to_free_edge_gpos
) {
	for (int dir{ 0 }; dir < Direction::MAX; ++dir) {
		if (blocked_sides.has(dir)) {
			continue;
		}

		Vector2i edge_gpos{ internal_gpos };

		if (dir == Direction::UP) {
			edge_gpos.y -= 1;

		} else if (dir == Direction::DOWN) {
			edge_gpos.y += rand_g_size.y;

		} else if (dir == Direction::LEFT) {
			edge_gpos.x -= 1;

		} else if (dir == Direction::RIGHT) {
			edge_gpos.x += rand_g_size.x;
		}

		if (
			edge_gpos.x < 0 || edge_gpos.y < 0 ||
			edge_gpos.x >= s_seg_g_size.x || edge_gpos.y >= s_seg_g_size.y
		) {
			continue;
		}

		dir_to_free_edge_gpos[dir].push_back(Edge{ edge_gpos, rand_g_size });
	}
}

void Zone::add_dir_size_to_gpos(
	std::array<uint64_t, Direction::MAX> &dir_size_occ,
	std::array<LocalVector<Vector2i>, FLAT_TREE_SIZE> &dir_size_to_gpos,
	const int req_dir_offset,
	Direction req_dir,
	const LocalVector<Rect2i> &areas,
	int start_i
) {
	// there are now smaller areas
	for (int i{ start_i }; i < static_cast<int>(areas.size()); ++i) {
		const Rect2i &area{ areas[i] };
		const int s_i{ get_size_i(area.size) };
		ERR_FAIL_COND(s_i == -1);

		dir_size_to_gpos[req_dir_offset + s_i].push_back(area.position);
		dir_size_occ[req_dir] |= 1ull << s_i;
	}
}

bool Zone::try_place_s_region(
	Ref<RandomNumberGenerator> rng,
	std::array<uint64_t, Direction::MAX> &dir_size_occ,
	FlatDirSizeToGposArr &dir_size_to_gpos,
	Ref<PCG> pcg,
	DirEdge &dir_to_free_edge_gpos,
	Ref<BitGrid2D> gen_occupancy,
	int w_seg
) {
	bool is_success{ false };

	Vector2i rand_g_size{ g_size };
	Vector2i rand_g_size_inc{ g_size_inclusive };

	if (rand_length_addition.x > 0) {
		const int x_addition{ rng->randi_range(0, rand_length_addition.x) };
		rand_g_size.x += x_addition;
		rand_g_size_inc.x += x_addition;
	}
	if (rand_length_addition.y > 0) {
		const int y_addition{ rng->randi_range(0, rand_length_addition.y) };
		rand_g_size.y += y_addition;
		rand_g_size_inc.y += y_addition;
	}

	const int64_t side_count{ joining_sides.size() }; // >0 sides enforced at creation
	const int start_dir_i{ rng->randi_range(0, side_count - 1) };

	for (int64_t dir_offset{ 0 }; dir_offset < side_count; ++dir_offset) {
		const int wrapped_dir{ (start_dir_i + dir_offset) % side_count };
		const int dir_i{ joining_sides[wrapped_dir] };
		const Direction dir{ static_cast<Direction>(dir_i) };
		const Direction req_dir{ invert_direction(dir) };

		LocalVector<Edge> &free_edge_gpos{ dir_to_free_edge_gpos[req_dir] };

		////////////////////////////////////////////////////////////////////////////
		//// find >= current size in the tree of previously searched free sides ////
		//// and check if they are still empty before placing                   ////
		////////////////////////////////////////////////////////////////////////////

		// find the first cached free side size that is >= required size
		// try this side to ensure its free, if not then try the larger sizes
		const uint64_t occupancy{ dir_size_occ[req_dir] };
		const int min_cell_count{ rand_g_size_inc.x * rand_g_size_inc.y };

		// flat packed direction array offsets
		const int req_dir_offset{ req_dir * MAX_CELL_COUNT };

		// advance larger and larger until at the max or nothing is found
		for (int cell_i{ min_cell_count }; cell_i < MAX_CELL_COUNT; ++cell_i) {
			cell_i = get_size_or_larger_i(occupancy, cell_i);
			if (cell_i == -1) {
				break;
			}

			// for every free grid position at this size in this direction
			const Vector2i free_size{ (cell_i % MAX_G_SIZE.x) + 1, (cell_i / MAX_G_SIZE.x) + 1 };
			LocalVector<Vector2i> &free_gpos_arr{ dir_size_to_gpos[req_dir_offset + cell_i] };

			// iterate backwards so we can safely remove items as we go
			for (int i{ free_gpos_arr.size() }; i > 0; --i) {
				
				// get and move free grid position from region edge to potential placement position
				Vector2i free_gpos{ free_gpos_arr[i] };
				if (dir == Direction::DOWN) {
					free_gpos.y -= free_size.y - 1;
				} else if (dir == Direction::RIGHT) {
					free_gpos.x -= free_size.x - 1;
				}

				const bool is_added{
					try_add_region_to_edge(dir, free_gpos, rand_g_size_inc, rng, gen_occupancy)
				};
			}
		}

		
	// SAFETY ITERATIONS... 

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
			Vector2i wanted_size{ rand_g_size_inc };

			// different rules for 1 length if there are stone sides because it will never fit
			if (req_dir == Direction::UP) {
				search_origin.y -= 7;

				if (prev_g_size.x == 1 && blocked_sides.has(Direction::LEFT)) {
					search_size.x = 1;
					wanted_size = Vector2i(0, 0);
				}

				else {
					search_size.x = prev_g_size.x + rand_g_size.x;
					if (!blocked_sides.has(Direction::RIGHT)) {
						search_size.x -= 1;
					}
				}
				
			} else if (req_dir == Direction::DOWN) {
				if (prev_g_size.x == 1 && blocked_sides.has(Direction::RIGHT)) {
					search_size.x = 1;
					wanted_size = Vector2i(0, 0);
				}

				else {
					search_origin.x -= rand_g_size.x; 
					search_size.x = prev_g_size.x + rand_g_size.x;
					
					if (!blocked_sides.has(Direction::LEFT)) {
						search_origin.x += 1;
						search_size.x -= 1;
					}
				}

			} else if (req_dir == Direction::LEFT) {;
				search_origin.x -= 7;

				if (prev_g_size.y == 1 && blocked_sides.has(Direction::UP)) {
					search_size.y = 1;
					wanted_size = Vector2i(0, 0);
				}

				else {
					search_origin.y -= rand_g_size.y;
					search_size.y = prev_g_size.y + rand_g_size.y;
					
					if (!blocked_sides.has(Direction::UP)) {
						search_origin.y += 1;
						search_size.y -= 1;
					}
				}

			} else if (req_dir == Direction::RIGHT) {
				if (prev_g_size.y == 1 && blocked_sides.has(Direction::UP)) {
					search_size.y = 1;
					wanted_size = Vector2i(0, 0);
				}

				else {
					search_size.y = prev_g_size.y + rand_g_size.y;
					if (!blocked_sides.has(Direction::DOWN)) {
						search_size.y -= 1;
					}
				}
			}

			////////////////////////////
			//////// MOVE ME ///////////
			////////////////////////////
			BitGrid2D::Direction bit_dir{ static_cast<BitGrid2D::Direction>(dir) };
			LocalVector<Rect2i> org_size{
				gen_occupancy->find_largest_anchored_areas_in_area(
					search_origin, search_size, bit_dir, rng, rand_g_size_inc
				)
			};

			// edge is full
			if (org_size.size() == 0) {
				// edge cant be used so remove it
				free_edge_gpos[gpos_i] = free_edge_gpos[free_edge_gpos.size() - 1];
				free_edge_gpos.resize(free_edge_gpos.size() - 1);
				continue;
			}

			// there is enough room
			if (org_size[0].size == rand_g_size_inc) {
				Vector2i dir_offset_gpos{ org_size[0].position };
				if (dir == Direction::DOWN) {
					dir_offset_gpos.y -= rand_g_size_inc.y - 1;
				} else if (dir == Direction::RIGHT) {
					dir_offset_gpos.x -= rand_g_size_inc.x - 1;
				}

				free_edge_gpos[gpos_i] = free_edge_gpos[free_edge_gpos.size() - 1];
				free_edge_gpos.resize(free_edge_gpos.size() - 1);

				// is this even segment position, or is it in the search space in some way
				if (is_debug) {
					debug_region(dir_offset_gpos, rand_g_size_inc, this, w_seg);
				}
				add_region(rng, pcg, dir_offset_gpos, rand_g_size, dir_to_free_edge_gpos);

				// edge is being used so remove it
				if (org_size.size() > 1) {
					add_dir_size_to_gpos(
						dir_size_occ, dir_size_to_gpos,
						req_dir_offset, req_dir,
						org_size, 1
					);
				}
				return true;
			}

			// we did not find enough room
			add_dir_size_to_gpos(
				dir_size_occ, dir_size_to_gpos,
				req_dir_offset, req_dir,
				org_size, 0
			);
		}
	}

	return false;
}

void Zone::add_region(
	Vector2i external_gpos, Vector2i _exclusive_g_size, DirEdge &dir_to_free_edge_gpos
) {
	Vector2i internal_gpos{
		blocked_sides.has(Direction::LEFT) ? external_gpos.x + 1 : external_gpos.x,
		blocked_sides.has(Direction::UP) ? external_gpos.y + 1 : external_gpos.y
	};
	
	fill_blocked_edges(internal_gpos, _exclusive_g_size, rng, pcg);

	const int layer_offset{ Tile::BACKGROUND * m_seg_cell_count };
	pcg->add_tile_rect(layer_offset, Tile::DUG, internal_gpos, _exclusive_g_size, false, rng);

	fill_internal(internal_gpos, _exclusive_g_size, rng, pcg);

	pcg->generative_occupancy->set_area(internal_gpos, _exclusive_g_size); // ensure dug doesnt get overwritten

	add_free_edge_gpos(internal_gpos, _exclusive_g_size, dir_to_free_edge_gpos);
}

void Zone::fill_internal(Vector2i w_internal_gpos, Vector2i rand_g_size) {
	for (const InternalChoiceSet &choice_sets : internal_choices) {

		// weighted random choice
		float rand_f{ rng->randf() };
		int choice_i{ 0 };
		for (; choice_i < choice_sets.norm_weights.size(); ++choice_i) {
			if (choice_sets.norm_weights[choice_i] >= rand_f) {
				break;
			}
		}
		if (choice_i >= choice_sets.norm_weights.size()) {
			choice_i = choice_sets.norm_weights.size() - 1;
		}
		ERR_FAIL_INDEX(choice_i, static_cast<int>(choice_sets.choice_set.size()));
		InternalEntry choice{ choice_sets.choice_set[choice_i] };

		Vector2i seg_placement_gpos{ 0, 0 };

		if (choice.placement == Placement::FORCE_GPOS) {
			Vector2i gpos{ w_internal_gpos };
			if (choice.gpos_alignment.x < 0) {
				gpos.x += rand_g_size.x;
			}
			else {
				gpos.x -= 1;
			}

			if (choice.gpos_alignment.y < 0) {
				gpos.y += rand_g_size.y;
			}
			else {
				gpos.y -= 1;
			}

			gpos += choice.gpos_alignment;
			
			if (choice.type == InternalEntry::TYPE_CALLABLE) {
				choice.callable.call(gpos);
			} else {
				pcg->add_gpos_tile(choice.layer_offset, choice.tile_index, gpos, true, rng);
			}
			continue;

		} else if (choice.placement == Placement::FILL) {
			Vector2i limit{ rand_g_size - choice.size + Vector2i(1, 1) };
			Vector2i offset_gpos{ w_internal_gpos };

			if (choice.gpos_alignment == A_UP) {
				limit.y = 1;
			} else if (choice.gpos_alignment == A_DOWN) {
				limit.y = 1;
				offset_gpos.y += rand_g_size.y - choice.size.y;
			} else if (choice.gpos_alignment == A_LEFT) {
				limit.x = 1;
			} else if (choice.gpos_alignment == A_RIGHT) {
				limit.x = 1;
				offset_gpos.x += rand_g_size.x - choice.size.x;
			}

			for (int x{ 0 }; x < limit.x; x += choice.size.x) {
				for (int y{ 0 }; y < limit.y; y += choice.size.y) {
					try_place_internal(choice, offset_gpos + Vector2i(x, y), pcg, rng);
				}
			}

			continue;

		} else if (choice.placement == Placement::CENTER) {
			if (choice.gpos_alignment == A_NONE) {
				seg_placement_gpos = rand_g_size / 2 - choice.size / 2;
			} else if (choice.gpos_alignment == A_UP) {
				seg_placement_gpos = Vector2i(rand_g_size.x / 2 - choice.size.x / 2, 0);
			} else if (choice.gpos_alignment == A_DOWN) {
				seg_placement_gpos = Vector2i(rand_g_size.x / 2 - choice.size.x / 2, rand_g_size.y - choice.size.y);
			} else if (choice.gpos_alignment == A_LEFT) {
				seg_placement_gpos = Vector2i(0, rand_g_size.y / 2 - choice.size.y / 2);
			} else if (choice.gpos_alignment == A_RIGHT) {
				seg_placement_gpos = Vector2i(rand_g_size.x - choice.size.x, rand_g_size.y / 2 - choice.size.y / 2);
			}

		} else if (choice.placement == Placement::END) {
			if (choice.gpos_alignment == A_UP) {
				seg_placement_gpos.x = rand_g_size.x - choice.size.x;
			} else if (
				choice.gpos_alignment == A_DOWN ||
				choice.gpos_alignment == A_RIGHT
			) {
				seg_placement_gpos = rand_g_size - choice.size;
			} else if (choice.gpos_alignment == A_LEFT) {
				seg_placement_gpos.y = rand_g_size.y - choice.size.y;
			}

		} else if (choice.placement == Placement::RANDOM) {

			BitGrid2D::Direction dir{};
			if (choice.gpos_alignment == A_NONE) { dir = BitGrid2D::NONE; }
			else if (choice.gpos_alignment == A_UP) { dir = BitGrid2D::UP; }
			else if (choice.gpos_alignment == A_DOWN) { dir = BitGrid2D::DOWN; }
			else if (choice.gpos_alignment == A_LEFT) { dir = BitGrid2D::LEFT; }
			else if (choice.gpos_alignment == A_RIGHT) { dir = BitGrid2D::RIGHT; }

			Vector2i unset_gpos{
				pcg->generative_occupancy->find_anchored_area_in_area(
					w_internal_gpos, rand_g_size, dir, choice.size, rng 
				)
			};

			if (unset_gpos == NOT_SET) {
				continue;
			}

			if (choice.type == InternalEntry::TYPE_CALLABLE) {
				choice.callable.call(unset_gpos);
			} else {
				pcg->add_gpos_tile(
					choice.layer_offset, choice.tile_index, unset_gpos, true, rng
				);
			}
			continue;

		} else if (choice.placement == Placement::START) {
			if (choice.gpos_alignment == A_DOWN) {
				seg_placement_gpos.y += rand_g_size.y - choice.size.y;
			}
			else if (choice.gpos_alignment == A_RIGHT) {
				seg_placement_gpos.x += rand_g_size.x - choice.size.x;
			}
		}
		try_place_internal(choice, w_internal_gpos + seg_placement_gpos, pcg, rng);
	}
}

void Zone::try_place_internal(
	InternalEntry choice, Vector2i gpos, Ref<PCG> pcg, Ref<RandomNumberGenerator> rng
) {
	if (!pcg->generative_occupancy->is_area_state(gpos, choice.size)) {
		return;
	}
	if (choice.type == InternalEntry::TYPE_CALLABLE) {
		choice.callable.call(gpos);
	} else {
		pcg->add_gpos_tile(choice.layer_offset, choice.tile_index, gpos, true, rng);
	}
}
