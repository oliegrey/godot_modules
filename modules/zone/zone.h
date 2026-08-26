#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/typed_array.h"
#include "modules/region/region.h"
#include "modules/direction/direction.h"

#include <array>

class Region;
class RandomNumberGenerator;
class PCG;
class BitGrid2D;

class Zone : public RefCounted {
	GDCLASS(Zone, RefCounted);

private:
	struct Edge {
		int has_blocked_side;
		int length;
		Vector2i gpos;

		Edge(int _has_blocked_side, int _length, const Vector2i &_gpos) :
				has_blocked_side{ _has_blocked_side }, length{ _length }, gpos{ _gpos }
			{}
	};

	// a tree to find grid positions that have needed free area sizes related to directions
	// dir * max size in cells + size in cells -> grid position
	class SizedEdgeCache {
	private:
		static std::array<std::array<uint64_t, 8>, 8> create_dominance_mask();
		inline static std::array<std::array<uint64_t, 8>, 8> s_dominance_mask{ create_dominance_mask() };
		inline static constexpr std::array<int, Direction::MAX> dir_offsets{
			Direction::UP * Region::MAX_CELL_COUNT,
			Direction::DOWN * Region::MAX_CELL_COUNT,
			Direction::LEFT * Region::MAX_CELL_COUNT,
			Direction::RIGHT * Region::MAX_CELL_COUNT
		};
		std::array<uint64_t, Direction::MAX> m_occ;
		std::array<LocalVector<Vector2i>, Region::MAX_CELL_COUNT * Direction::MAX> m_gpos;

	private:
		static int get_size_i(const Vector2i &p_size);

		static int get_gpos_i(const Direction::E dir, const int size_cell_i) {
			dir_offsets[dir] + size_cell_i;
		}

	public:
		int get_size_or_larger_i(Direction::E dir, const Vector2i size);
		int get_size_or_larger_i(Direction::E dir, const int cell_count);
		void add_free_rects(Direction::E dir, const LocalVector<Rect2i> &free_rects, int start_i);
		const LocalVector<Vector2i> &get_gpos(Direction::E dir, const int size_cell_i) {
			const int gpos_i{ get_gpos_i(dir, size_cell_i) };
			return m_gpos[gpos_i];
		}

		// returns whether the array is now empty //// remove_edge
		bool remove_free_rect(Direction::E dir, int size_cell_i, int rect_i) {
			const int gpos_i{ get_gpos_i(dir, size_cell_i) };
			LocalVector<Vector2i> &gpos{ m_gpos[gpos_i] };

			const int last_rect_i{ gpos.size() - 1 };
			gpos[rect_i] = gpos[last_rect_i];
			gpos.resize(last_rect_i);

			if (gpos.size() == 0) {
				m_occ[dir] &= ~(1ull << size_cell_i);
				return true;
			}
			return false;
		}
	};

private:
	inline static Vector2i s_seg_g_size;
	inline static int s_seg_cell_count;
	inline static bool s_is_debug;

	Ref<RandomNumberGenerator> m_rng;
	Ref<PCG> m_pcg;
	Ref<BitGrid2D> m_occ;
	int m_max_secondary_count;
	int m_w_seg;
	
	std::array<LocalVector<Edge>, Direction::MAX> dir_to_free_edges; // free grid position look up based on direction requirement; dir -> [free edge gpos, g_size, ...]
	SizedEdgeCache m_sized_edge_cache{};
	
private:
	void generate_primary();
	void generate_secondary();

	void add_region(Ref<Region> region, const Rect2i& region_rect);

	bool try_place_s_region(Ref<Region> region, const Region::Size& size);
	bool try_add_region_from_cached(
		Ref<Region> region, const Region::Size &size, const Direction::E in_dir
	);
	bool try_add_anchored_region(
		Ref<Region> region,
		const Vector2i &free_rect_gpos,
		const Region::Size &size,
		Direction::E in_dir
	);
	bool try_add_region_from_search(
		Ref<Region> region, const Region::Size &size, const Direction::E in_dir
	);

	void add_free_edges_to_cache(Ref<Region> region, const Rect2i &region_rect_e);

	bool remove_edge(
		LocalVector<Vector2i> &free_gpos_arr,
		int edge_i,
		uint64_t &dir_occupancy,
		int size_cell_i
	);

	void fill_blocked_sides(
		const LocalVector<Region::BlockedSide> &blocked_sides,
		const Rect2i &region_rect
	);
	void fill_corner(
		const LocalVector<Region::BlockedSide> &blocked_sides, const Vector2i& corner_gpos
	);

	void try_place_internal(const Region::InternalEntry &choice, const Vector2i &gpos);

	void add_dir_size_to_gpos(
		int req_dir_offset, Direction::E req_dir, const LocalVector<Rect2i> &areas, int start_i
	);

	void fill_internal(Ref<Region> region, const Rect2i &region_rect);
	
	void debug_region(Ref<Region> region, const Rect2i &region_rect_inc);

protected:
	void _bind_methods();

public:
	void initialize(Vector2i seg_g_size, bool is_debug = false);
	
	Ref<Zone> create(
		Ref<RandomNumberGenerator> rng, Ref<PCG> pcg, int max_secondary_count, int w_seg
	);
};
