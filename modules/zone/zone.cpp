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

void Region::_bind_methods() {
	
}

Ref<Zone> Zone::create(
	Ref<RandomNumberGenerator> rng, Ref<PCG> pcg, Vector2i seg_g_size, bool is_debug
) {
	Ref<Zone> zone;
	zone.instantiate();
	zone->rng = rng;
	zone->pcg = pcg;

	zone->m_seg_g_size = seg_g_size;
	zone->m_seg_cell_count = seg_g_size.x * seg_g_size.y;
	zone->is_debug = is_debug;
	zone->init_dominance_mask();
	return zone;
}

int Zone::get_region_relative_size_i(Vector2i size) {
	const int size_i{ (size.x - 1) + (size.y - 1) * Region::MAX_G_SIZE_X };
	ERR_FAIL_INDEX_V(size_i, 64, -1);
	return size_i;
}

void Zone::fill_blocked_sides(Ref<Region> region, const Rect2i &region_rect) {
	const LocalVector<int> &sides{ region->blocked_sides };
	const int side_count{ sides.size() };
	const int corner_count{ side_count * side_count * 0.25 };

	LocalVector<Rect2i> edge_rects;
	edge_rects.resize(side_count + corner_count);

	uint8_t corner_bitmap{ 0 };

	for (int i{ 0 }; i < side_count; ++i) {
		const int dir{ sides[i] };

		
		edge_rects[i] = Rect2i{ internal_gpos, exlusive_size };

		if (dir == Region::Direction::UP) {
			edge_rects[i].position.y -= 1;
			edge_rects[i].size.y = 1;
			
		} else if (dir == Region::Direction::DOWN) {
			edge_rects[i].position.y += exlusive_size.y;
			edge_rects[i].size.y = 1;

		} else if (dir == Region::Direction::LEFT) {
			edge_rects[i].position.x -= 1;
			edge_rects[i].size.x = 1;

		} else if (dir == Region::Direction::RIGHT) {
			edge_rects[i].position.x += exlusive_size.x;
			edge_rects[i].size.x = 1;
		}


		pcg->rand_fill_rect(rng, fill, tiles_i, layer_offsets, gpos, a);

		fill_blocked(fill, blocked_gpos, blocked_rect);

		corner_bitmap |= 1 << dir;
	}
	if ((corner_bitmap & 0b0101) == 0b0101) { // fill top left corner
		Vector2i blocked_gpos{ internal_gpos + Vector2i(-1, -1) };
		fill_blocked(BlockedFill::ANY, blocked_gpos, Vector2i(1, 1), true);
	}
	if ((corner_bitmap & 0b0110) == 0b0110) { // fill bottom left corner
		Vector2i blocked_gpos{ internal_gpos + Vector2i(-1, exlusive_size.y) };
		fill_blocked(BlockedFill::ANY, blocked_gpos, Vector2i(1, 1), true);
	}
	if ((corner_bitmap & 0b1010) == 0b1010) { // fill bottom right corner
		Vector2i blocked_gpos{ internal_gpos + exlusive_size };
		fill_blocked(BlockedFill::ANY, blocked_gpos, Vector2i(1, 1), true);
	}
	if ((corner_bitmap & 0b1001) == 0b1001) { // fill top right corner
		Vector2i blocked_gpos{ internal_gpos + Vector2i(exlusive_size.x, -1) };
		fill_blocked(BlockedFill::ANY, blocked_gpos, Vector2i(1, 1), true);
	}
}
