#include "zone.h"
#include "modules/pcg/pcg.h"
#include "modules/bit_grid_2d/bit_grid_2d.h"
#include "modules/tile/Tile.h"
#include "core/math/random_number_generator.h"

#include "scene/gui/label.h"
#include "scene/gui/color_rect.h"
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

std::array<std::array<uint64_t, 8>, 8> Zone::SizedEdgeCache::create_dominance_mask_64() {
	std::array<std::array<uint64_t, 8>, 8> masks;
	constexpr int SIZE{ 8 };
	for (int tx{ 0 }; tx < SIZE; ++tx) {
		for (int ty{ 0 }; ty < SIZE; ++ty) {
			uint64_t mask{ 0 };
			for (int x{ tx + 1 }; x <= SIZE; ++x) {
				for (int y{ ty + 1 }; y <= SIZE; ++y) {
					const int s_i{ get_size_i(Vector2i{ x, y }) };
					DEV_ASSERT(s_i != -1);
					mask |= 1ull << s_i;
				}
			}
			masks[tx][ty] = mask;
		}
	}
	return masks;
}

int Zone::SizedEdgeCache::get_size_i(const Vector2i &p_size) {
	if (p_size.x < 1 || p_size.x > 8 || p_size.y < 1 || p_size.y > 8) {
		return -1;
	}
	return (p_size.x - 1) * 8 + (p_size.y - 1);
}

int Zone::SizedEdgeCache::get_size_or_larger_i(Direction::E dir, const Vector2i size) {
	DEV_ASSERT(size.x <= 8 && size.y <= 8);
	const uint64_t mask{ s_dominance_mask[size.x - 1][size.y - 1] };
	const uint64_t masked_bitmap{ m_occ[dir] & mask };
	if (masked_bitmap == 0) {
		return -1;
	}
	return ctz64(masked_bitmap);
}

int Zone::SizedEdgeCache::get_size_or_larger_i(Direction::E dir, const int cell_count) {
	return get_size_or_larger_i(dir, Vector2i{ cell_count % 8, cell_count / 8 });
}

void Zone::SizedEdgeCache::add_free_rects(
	Direction::E dir, const LocalVector<Rect2i> &free_rects, int start_i
) {
	const int free_rects_size{ static_cast<int>(free_rects.size()) };
	for (int i{ start_i }; i < free_rects_size; ++i) {
		const Rect2i &free_rect{ free_rects[i] };
		const int s_i{ get_size_i(free_rect.size) };
		DEV_ASSERT(s_i != -1);

		m_gpos[dir_offsets[dir] + s_i].push_back(free_rect.position);
		m_occ[dir] |= 1ull << s_i;
	}
}

// returns whether the array is now empty
bool Zone::SizedEdgeCache::remove_free_rect(Direction::E dir, int size_cell_i, int rect_i) {
	const int gpos_i{ get_gpos_i(dir, size_cell_i) };
	LocalVector<Vector2i> &gpos{ m_gpos[gpos_i] };

	const int last_rect_i{ static_cast<int>(gpos.size()) - 1 };
	gpos[rect_i] = gpos[last_rect_i];
	gpos.resize(last_rect_i);

	if (gpos.size() == 0) {
		m_occ[dir] &= ~(1ull << size_cell_i);
		return true;
	}
	return false;
}

const LocalVector<Vector2i> &Zone::SizedEdgeCache::get_gpos(
	Direction::E dir, const int size_cell_i
) {
	const int gpos_i{ get_gpos_i(dir, size_cell_i) };
	return m_gpos[gpos_i];
}

int Zone::SizedEdgeCache::get_gpos_i(const Direction::E dir, const int size_cell_i) {
	return dir_offsets[dir] + size_cell_i;
}

void Zone::_bind_methods() {
	ClassDB::bind_static_method(
		"Zone", D_METHOD("initialize", "seg_g_size", "is_debug"), &Zone::initialize
	);

	ClassDB::bind_static_method(
		"Zone", D_METHOD("create", "rng", "pcg", "max_secondary_count", "w_seg"), &Zone::create
	);
}

void Zone::initialize(Vector2i seg_g_size, bool is_debug) {
	s_seg_g_size = seg_g_size;
	s_seg_cell_count = seg_g_size.x * seg_g_size.y;
	s_is_debug = is_debug;
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
	zone->m_occ = pcg->generative_occupancy;
	zone->m_max_secondary_count = max_secondary_count;
	zone->m_w_seg = w_seg;
	for (int dir{ 0 }; dir < Direction::MAX; ++dir) {
		zone->dir_to_free_edges[dir].reserve(max_secondary_count * 4 + 4);
	}
	zone->generate_primary();
	zone->generate_secondary();
	return zone;
}

void Zone::generate_primary() {
	// get a random weighted primary region // CRASH HERE
	const int p_threshold_i{ Region::get_slot_threshold_i(Region::PRIMARY, m_w_seg) }; // region cutoff (uses ordered arr)
	if (p_threshold_i == -1) {
		return;
	}
	const float p_weights_sum{ Region::get_slot_weights_sum(Region::PRIMARY, p_threshold_i) };
	Ref<Region> p_region{ Region::get_slot_rand_region(
		Region::PRIMARY, p_threshold_i, p_weights_sum, m_rng->randf()
	)};

	// randomly extend grid size of region
	Region::Size ext_size{ p_region->size };
	ext_size += Vector2i{
		m_rng->randi_range(0, p_region->rand_length_addition.x),
		m_rng->randi_range(0, p_region->rand_length_addition.y)
	};

	// find a free area that will fit the region, starting the search at a random offset
	int rand_cell_i{ m_rng->randi_range(0, s_seg_cell_count - 1) };
	const int p_cell_i{ m_occ->find_area_in_grid(ext_size.i, rand_cell_i, rand_cell_i - 1) };
	ERR_FAIL_COND(p_cell_i == -1);
	auto p_gpos{ Vector2i(p_cell_i % s_seg_g_size.x, p_cell_i / s_seg_g_size.x) };

	add_region(p_region, { p_gpos, ext_size.e });
}


void Zone::add_region(Ref<Region> region, const Rect2i& region_rect_e) {
	fill_blocked_sides(region->blocked_sides, region_rect_e);

	m_pcg->add_tile_rect(Tile::BACKGROUND * s_seg_cell_count, Tile::DUG, region_rect_e, false, m_rng);

	fill_internal(region, region_rect_e);

	m_pcg->generative_occupancy->set_rect(region_rect_e); // ensure dug doesnt get overwritten

	add_free_edges_to_cache(region, region_rect_e);

	if (s_is_debug) {
		debug_region(region, region_rect_e);
	}
}

void Zone::add_free_edges_to_cache(Ref<Region> region, const Rect2i &region_rect_e) {
	for (int out_dir{ 0 }; out_dir < Direction::MAX; ++out_dir) {
		if (!region->free_sides.has(static_cast<Direction::E>(out_dir))) {
			continue;
		}

		int has_blocked_origin;
		int edge_length;
		if (out_dir == Direction::UP || out_dir == Direction::DOWN) {
			has_blocked_origin = static_cast<int>(!region->free_sides.has(Direction::LEFT));
			edge_length = region_rect_e.size.x;
		} else {
			has_blocked_origin = static_cast<int>(!region->free_sides.has(Direction::UP));
			edge_length = region_rect_e.size.y;
		}

		Vector2i edge_gpos{ region_rect_e.position };
		if (out_dir == Direction::UP) {
			edge_gpos.y -= 1;
		} else if (out_dir == Direction::DOWN) {
			edge_gpos.y += region_rect_e.size.y + static_cast<int>(!region->free_sides.has(Direction::DOWN));
		} else if (out_dir == Direction::LEFT) {
			edge_gpos.x -= 1;
		} else if (out_dir == Direction::RIGHT) {
			edge_gpos.x += region_rect_e.size.x + static_cast<int>(!region->free_sides.has(Direction::RIGHT));
		}

		if (
			edge_gpos.x < 0 || edge_gpos.y < 0 ||
			edge_gpos.x >= s_seg_g_size.x || edge_gpos.y >= s_seg_g_size.y
		) {
			continue;
		}

		dir_to_free_edges[out_dir].push_back(Edge{ has_blocked_origin, edge_length, edge_gpos });
	}
}

void Zone::generate_secondary() {
	// get random secondary count in bounds
	const int target_secondary_count{
		s_is_debug ? m_max_secondary_count :
		m_rng->randi_range(m_max_secondary_count / 4, m_max_secondary_count)
	};

	LocalVector<Ref<Region>> s_regions;
	s_regions.resize(target_secondary_count);

	LocalVector<Region::Size> s_sizes;
	s_sizes.resize(target_secondary_count);

	// randomly select regions and popular s_regions
	const int s_threshold_i{ Region::get_slot_threshold_i(Region::SECONDARY, m_w_seg) }; // region cutoff (uses ordered arr)
	if (s_threshold_i == -1) {
		return;
	}
	const float s_weights_sum{ Region::get_slot_weights_sum(Region::SECONDARY, s_threshold_i) };

	for (int i{ 0 }; i < target_secondary_count; ++i) {
		const float rf{ m_rng->randf() };
		s_regions[i] = Region::get_slot_rand_region(Region::SECONDARY, s_threshold_i, s_weights_sum, rf);

		s_sizes[i] = Region::Size{ s_regions[i]->size };
		s_sizes[i] += Vector2i{
			m_rng->randi_range(0, s_regions[i]->rand_length_addition.x),
			m_rng->randi_range(0, s_regions[i]->rand_length_addition.y)
		};
	}

	// attempt to place regions repeatedly, new free edges open with each success
	const int MAX_ATTEMPTS{ 4 };
	for (int attempt{ 0 }; attempt < MAX_ATTEMPTS; ++attempt) {
		const int region_count{ static_cast<int>(s_regions.size()) };

		for (int i{ region_count - 1 }; i >= 0; --i) {
			bool is_placed{ try_place_s_region(s_regions[i], s_sizes[i]) };

			if (is_placed) {
				s_regions[i] = s_regions[region_count - 1];
				s_sizes[i] = s_sizes[region_count - 1];
				s_regions.resize(region_count - 1);
				s_sizes.resize(region_count - 1);
			}
		}
	}
}

bool Zone::try_place_s_region(Ref<Region> region, const Region::Size& size) {
	bool is_added{ false };

	const LocalVector<Direction::E> &joining_sides{ region->joining_sides };
	const int side_count{ static_cast<int>(joining_sides.size()) }; // >0 sides enforced at creations

	const int start_dir_i{ m_rng->randi_range(0, side_count - 1) };
	for (int dir_offset{ 0 }; dir_offset < side_count; ++dir_offset) {
		const int wrapped_dir{ (start_dir_i + dir_offset) % side_count };

		const Direction::E out_dir{ joining_sides[wrapped_dir] }; 
		const Direction::E in_dir{ Direction::invert(out_dir) };

		is_added = try_add_region_from_cached(region, size, in_dir);

		if (is_added) {
			break;
		}

		is_added = try_add_region_from_search(region, size, in_dir);
	}

	return is_added;
}

bool Zone::try_add_region_from_cached(
	Ref<Region> region, const Region::Size &size, const Direction::E in_dir
) {
	bool is_added{ false };

	const int min_cell_count{ size.i.x * size.i.y };
	// advance larger and larger until at the max or nothing is found
	for (int size_cell_i{ min_cell_count }; size_cell_i < Region::MAX_CELL_COUNT; ++size_cell_i) {
		size_cell_i = m_sized_edge_cache.get_size_or_larger_i(in_dir, size_cell_i);
		if (size_cell_i == -1) {
			break;
		}

		// for every free grid position at this size in this direction
		const Vector2i free_size{
			(size_cell_i % Region::MAX_G_SIZE_X) + 1, (size_cell_i / Region::MAX_G_SIZE_X) + 1
		};
		const LocalVector<Vector2i> &free_gpos_arr{ m_sized_edge_cache.get_gpos(in_dir, size_cell_i) };

		// iterate backwards so we can safely remove items as we go
		for (int rect_i{ static_cast<int>(free_gpos_arr.size()) - 1 }; rect_i > 0; --rect_i) {
				
			// get and move free grid position from region edge to top left of free rect
			Vector2i free_gpos{ free_gpos_arr[rect_i] };
			if (in_dir == Direction::DOWN) {
				free_gpos.y -= free_size.y - 1;
			} else if (in_dir == Direction::LEFT) {
				free_gpos.x -= free_size.x - 1;
			}

			is_added = try_add_anchored_region(region, free_gpos, size, in_dir);
			m_sized_edge_cache.remove_free_rect(in_dir, size_cell_i, rect_i); // in every case, the edge is consumed
			if (is_added) {
				break;
			}
		}
	}

	return is_added;
}

// consumes edge if it is not free to use, adds new smaller areas if possible
// removes the final edge element if not free
bool Zone::try_add_anchored_region(
	Ref<Region> region,
	const Vector2i &free_rect_gpos,
	const Region::Size &size,
	Direction::E in_dir
) {
	// check that it is free (fast if it is free and prevents overlapping regions for rechecks)
	LocalVector<Rect2i> free_rects{
		m_occ->find_largest_anchored_areas_in_area(free_rect_gpos, size.i, in_dir, m_rng, size.i)
	};

	bool is_free{ free_rects[0].size == size.i };

	if (is_free) {
		Vector2i placement_position{ free_rects[0].position };

		// move from free edge position to placement position
		if (in_dir == Direction::DOWN) {
			placement_position.y -= size.i.y - 1;
		} else if (in_dir == Direction::LEFT) {
			placement_position.x -= size.i.x - 1;
		}
		
		add_region(region, { placement_position, size.e });
	}

	// if more than one free size found add all from the beginning or after exact match
	const int start_i{ static_cast<int>(is_free) };
	const int free_rects_size{ static_cast<int>(free_rects.size()) };
	if (free_rects_size > start_i) {
		m_sized_edge_cache.add_free_rects(in_dir, free_rects, start_i);
	}

	return is_free;
}

bool Zone::try_add_region_from_search(
	Ref<Region> region, const Region::Size &size, const Direction::E in_dir
) {
	LocalVector<Edge> &edges{ dir_to_free_edges[in_dir] }; // inverse direction for mate

	// iterate backwards so we can swap remove items from the end without issues
	for (int p_i{ static_cast<int>(edges.size()) - 1 }; p_i >= 0 ; --p_i) {
		const Edge &edge{ edges[p_i] };
		Vector2i search_size{ Region::MAX_G_SIZE_X, Region::MAX_G_SIZE_Y };
		Vector2i search_origin{ edge.gpos };

		int axis{ in_dir / 2 };
		search_size[axis] = region->size.i[axis] + region->size.e[axis] - 2 + edge.length;
		search_origin[axis] -= region->size.e[axis] - edge.has_blocked_origin; // origin should be along edge as histogram is agnostic

		LocalVector<Rect2i> free_rects_i{
			m_occ->find_largest_anchored_areas_in_area(
				search_origin, search_size, in_dir, m_rng, size.i
			)
		};

		// edge is always consumed: either added to size cache, used or unusable
		edges[p_i] = edges[edges.size() - 1];
		edges.resize(edges.size() - 1);

		// there is enough room for region
		if (free_rects_i[0].size == size.i) {

			// histogram returns anchored position so we must transform it to the exclusive region position
			Vector2i region_gpos_e{ free_rects_i[0].position };
			if (in_dir == Direction::DOWN) {
				region_gpos_e.y -= size.e.y - 1;
			} else if (in_dir == Direction::RIGHT) {
				region_gpos_e.x -= size.e.x - 1;
			}
			if (axis == 0 && !region->free_sides.has(Direction::LEFT)) {
				++region_gpos_e.x;
			}
			else if (axis == 1 && !region->free_sides.has(Direction::UP)) {
				++region_gpos_e.y;
			}

			add_region(region, Rect2i{ region_gpos_e, size.e });

			// we found remaining fractional space
			if (free_rects_i.size() > 1) {
				m_sized_edge_cache.add_free_rects(in_dir, free_rects_i, 1);
			}
			return true;
		}
		// we ONLY found fractional space
		else if (free_rects_i.size() > 0) {
			m_sized_edge_cache.add_free_rects(in_dir, free_rects_i, 0);
		}
	}

	return false;
}

void Zone::fill_blocked_sides(
	const LocalVector<Region::BlockedSide> &blocked_sides,
	const Rect2i &region_rect_e
) {
	const int side_count{ static_cast<int>(blocked_sides.size()) };
	const int corner_count{ side_count * side_count / 4 };

	uint8_t corner_bitmap{ 0 };

	for (int i{ 0 }; i < side_count; ++i) {
		const Region::BlockedSide &blocked_side{ blocked_sides[i] };
		const int dir{ blocked_side.direction };

		Rect2i edge_rect{ region_rect_e };

		if (dir == Direction::UP) {
			edge_rect.position.y -= 1;
			edge_rect.size.y = 1;
			
		} else if (dir == Direction::DOWN) {
			edge_rect.position.y += region_rect_e.size.y;
			edge_rect.size.y = 1;

		} else if (dir == Direction::LEFT) {
			edge_rect.position.x -= 1;
			edge_rect.size.x = 1;

		} else if (dir == Direction::RIGHT) {
			edge_rect.position.x += region_rect_e.size.x;
			edge_rect.size.x = 1;
		}

		m_pcg->rand_fill_rect(m_rng, blocked_side.fill, blocked_side.tiles, edge_rect);
		corner_bitmap |= 1 << dir;
	}
	if ((corner_bitmap & 0b0101) == 0b0101) { // top left corner
		Vector2i corner_gpos{ region_rect_e.position + Vector2i{ -1, -1 } };
		fill_corner(blocked_sides, corner_gpos);
	}
	if ((corner_bitmap & 0b0110) == 0b0110) { // bottom left corner
		Vector2i corner_gpos{ region_rect_e.position + Vector2i{ -1, region_rect_e.size.y } };
		fill_corner(blocked_sides, corner_gpos);
	}
	if ((corner_bitmap & 0b1010) == 0b1010) { // bottom right corner
		Vector2i corner_gpos{ region_rect_e.position + region_rect_e.size };
		fill_corner(blocked_sides, corner_gpos);
	}
	if ((corner_bitmap & 0b1001) == 0b1001) { // top right corner
		Vector2i corner_gpos{ region_rect_e.position + Vector2i{ region_rect_e.size.x, -1 } };
		fill_corner(blocked_sides, corner_gpos);
	}
}

void Zone::fill_corner(
	const LocalVector<Region::BlockedSide> &blocked_sides, const Vector2i& corner_gpos
) {
	const int i{ m_rng->randi_range(0, 1) ? Direction::UP : Direction::LEFT };
	const LocalVector<Ref<Tile>> &tiles{ blocked_sides[i].tiles };
	m_pcg->rand_fill_rect(m_rng, PCG::Fill::PICK_ONE, tiles, { corner_gpos, { 1, 1 } }, true);
}

void Zone::fill_internal(Ref<Region> region, const Rect2i &region_rect_e) {
	for (const Region::InternalChoiceSet &choice_sets : region->internal_choices) {

		// weighted random choice
		float rand_f{ m_rng->randf() };
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
		Region::InternalEntry choice{ choice_sets.choice_set[choice_i] };

		Vector2i seg_placement_gpos{ 0, 0 };

		if (choice.placement == Region::Placement::FORCE_GPOS) {
			Vector2i gpos{ region_rect_e.position };
			if (choice.gpos_alignment.x < 0) {
				gpos.x += region_rect_e.size.x;
			}
			else {
				gpos.x -= 1;
			}

			if (choice.gpos_alignment.y < 0) {
				gpos.y += region_rect_e.size.y;
			}
			else {
				gpos.y -= 1;
			}

			gpos += choice.gpos_alignment;
			
			if (choice.type == Region::InternalEntry::TYPE_CALLABLE) {
				choice.callable.call(gpos);
			} else {
				const int layer_offset{ choice.tile->layer * s_seg_cell_count };
				m_pcg->add_gpos_tile(layer_offset, choice.tile->tile, gpos, true, m_rng);
			}
			continue;

		} else if (choice.placement == Region::Placement::FILL) {
			Vector2i limit{ region_rect_e.size - choice.size + Vector2i(1, 1) };
			Vector2i offset_gpos{ region_rect_e.position };

			if (choice.alignment == Direction::UP) {
				limit.y = 1;
			} else if (choice.alignment == Direction::DOWN) {
				limit.y = 1;
				offset_gpos.y += region_rect_e.size.y - choice.size.y;
			} else if (choice.alignment == Direction::LEFT) {
				limit.x = 1;
			} else if (choice.alignment == Direction::RIGHT) {
				limit.x = 1;
				offset_gpos.x += region_rect_e.size.x - choice.size.x;
			}

			for (int x{ 0 }; x < limit.x; x += choice.size.x) {
				for (int y{ 0 }; y < limit.y; y += choice.size.y) {
					try_place_internal(choice, offset_gpos + Vector2i(x, y));
				}
			}

			continue;

		} else if (choice.placement == Region::Placement::CENTER) {
			if (choice.alignment == Direction::NONE) {
				seg_placement_gpos = region_rect_e.size / 2 - choice.size / 2;
			} else if (choice.alignment == Direction::UP) {
				seg_placement_gpos = Vector2i{
					region_rect_e.size.x / 2 - choice.size.x / 2, 0
				};
			} else if (choice.alignment == Direction::DOWN) {
				seg_placement_gpos = Vector2i{
					region_rect_e.size.x / 2 - choice.size.x / 2,
					region_rect_e.size.y - choice.size.y
				};
			} else if (choice.alignment == Direction::LEFT) {
				seg_placement_gpos = Vector2i{
					0, region_rect_e.size.y / 2 - choice.size.y / 2
				};
			} else if (choice.alignment == Direction::RIGHT) {
				seg_placement_gpos = Vector2i{
					region_rect_e.size.x - choice.size.x,
					region_rect_e.size.y / 2 - choice.size.y / 2
				};
			}

		} else if (choice.placement == Region::Placement::END) {
			if (choice.alignment == Direction::UP) {
				seg_placement_gpos.x = region_rect_e.size.x - choice.size.x;
			} else if (
				choice.alignment == Direction::DOWN ||
				choice.alignment == Direction::RIGHT
			) {
				seg_placement_gpos = region_rect_e.size - choice.size;
			} else if (choice.alignment == Direction::LEFT) {
				seg_placement_gpos.y = region_rect_e.size.y - choice.size.y;
			}

		} else if (choice.placement == Region::Placement::RANDOM) {

			Vector2i unset_gpos{
				m_pcg->generative_occupancy->find_anchored_area_in_area(
					region_rect_e.position,
					region_rect_e.size,
					choice.alignment,
					choice.size,
					m_rng 
				)
			};

			if (unset_gpos == NOT_SET) {
				continue;
			}

			if (choice.type == Region::InternalEntry::TYPE_CALLABLE) {
				choice.callable.call(unset_gpos);
			} else {
				const int l_offset{ choice.tile->layer * s_seg_cell_count };
				m_pcg->add_gpos_tile(l_offset, choice.tile->tile, unset_gpos, true, m_rng);
			}
			continue;

		} else if (choice.placement == Region::Placement::START) {
			if (choice.alignment == Direction::DOWN) {
				seg_placement_gpos.y += region_rect_e.size.y - choice.size.y;
			}
			else if (choice.alignment == Direction::RIGHT) {
				seg_placement_gpos.x += region_rect_e.size.x - choice.size.x;
			}
		}
		try_place_internal(choice, region_rect_e.position + seg_placement_gpos);
	}
}

void Zone::try_place_internal(const Region::InternalEntry &choice, const Vector2i &gpos) {
	if (!m_pcg->generative_occupancy->is_rect_state(Rect2i{ gpos, choice.size })) {
		return;
	}
	if (choice.type == Region::InternalEntry::TYPE_CALLABLE) {
		choice.callable.call(gpos);
	} else {
		const int layer_offset{ choice.tile->layer * s_seg_cell_count };
		m_pcg->add_gpos_tile(layer_offset, choice.tile->tile, gpos, true, m_rng);
	}
}

void Zone::debug_region(Ref<Region> region, const Rect2i &region_rect_inc) const {
	SceneTree *tree{ SceneTree::get_singleton() };
	Node *main{ tree->get_root()->get_node(NodePath("Main")) };

	Vector2 top_left;
	top_left.x = region_rect_inc.position.x * 50;
	top_left.y = (region_rect_inc.position.y + m_w_seg * s_seg_g_size.y) * 50;

	Vector2 size;
	size.x = region_rect_inc.size.x * 50;
	size.y = region_rect_inc.size.y * 50;

	uint32_t h{ region->name.hash() };
	Color color{ ((h & 0xFF)) / 255.0f, ((h >> 8) & 0xFF) / 255.0f, ((h >> 16) & 0xFF) / 255.0f, 0.20f };

	ColorRect *rect{ memnew(ColorRect) };
	rect->set_color(color);
	rect->set_position(top_left);
	rect->set_size(size);
	rect->set_z_index(998);
	main->call_deferred("add_child", rect);

	Label *label{ memnew(Label) };
	label->add_theme_font_size_override("font_size", 12);
	label->set_text(region->name);
	label->set_autowrap_mode(TextServer::AUTOWRAP_ARBITRARY);
	label->set_custom_minimum_size(Vector2(50, 50));
	label->set_position(top_left);
	label->set_z_index(999);
	main->call_deferred("add_child", label);
}
