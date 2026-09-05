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
		int has_blocked_origin;
		int length;
		Vector2i gpos;

		Edge() = default;

		// LENGTH IS EXCLUSIVE
		Edge(int _has_blocked_origin, int _length, const Vector2i &_gpos) :
				has_blocked_origin{ _has_blocked_origin }, length{ _length }, gpos{ _gpos }
			{}

		operator String() const {
			return to_string();
		}

		String to_string() const {
			return vformat("Edge(has_blocked_origin=%d, length=%d, gpos=%s)",
				has_blocked_origin, length, String(gpos));
		}
	};

	// a tree to find grid positions that have needed free area sizes related to directions
	// dir * max size in cells + size in cells -> grid position
	class SizedEdgeCache {
	private:
		static std::array<std::array<uint64_t, 8>, 8> create_dominance_mask_64();
		inline static std::array<std::array<uint64_t, 8>, 8> s_dominance_mask{ create_dominance_mask_64() };
		inline static constexpr std::array<int, Direction::MAX> dir_offsets{
			Direction::UP * Region::MAX_CELL_COUNT,
			Direction::DOWN * Region::MAX_CELL_COUNT,
			Direction::LEFT * Region::MAX_CELL_COUNT,
			Direction::RIGHT * Region::MAX_CELL_COUNT
		};
		std::array<uint64_t, Direction::MAX> m_occ;
		std::array<LocalVector<Vector2i>, Region::MAX_CELL_COUNT * Direction::MAX> m_gpos;

	public:
		static int get_size_i(const Vector2i &p_size);
		static Vector2i get_size(const int size_i);
		static int get_gpos_i(const Direction::E dir, const int size_cell_i);

		String to_string() {
			String edge_cache_str{"SizedEdgeCache("};

			for (int dir_i{ 0 }; dir_i < Direction::MAX; ++dir_i) {
				Direction::E dir{ static_cast<Direction::E>(dir_i) };
				const int offset{ dir_i * Region::MAX_CELL_COUNT };
				edge_cache_str += vformat("\n\tdirection %s: ", dir_i);

				for (int size_cell_i{ 0 }; size_cell_i < Region::MAX_CELL_COUNT; ++size_cell_i) {
					const int i{ offset + size_cell_i };
					const LocalVector<Vector2i> &gposes{ m_gpos[i] };
					if (gposes.size() <= 0) {
						continue;
					}

					edge_cache_str += "\n\t\t{ gpos[";
					for (Vector2i gpos : gposes) {
						edge_cache_str += vformat("%s, ", gpos);
					}
					edge_cache_str = edge_cache_str.left(edge_cache_str.size() - 3) + "], ";

					edge_cache_str += vformat("size%s", get_size(size_cell_i));

					edge_cache_str += "}, ";
				}
			}
			return edge_cache_str.left(edge_cache_str.size() - 3) + "\n)";
		}
		int get_size_or_larger_i(Direction::E dir, const Vector2i size);
		int get_size_or_larger_i(Direction::E dir, const int cell_count);
		void add_free_rects(Direction::E dir, const LocalVector<Rect2i> &free_rects, int start_i);
		const LocalVector<Vector2i> &get_gpos(Direction::E dir, const int size_cell_i);
		bool remove_free_rect(Direction::E dir, int size_cell_i, int rect_i);
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

public:
	inline static const Vector2i NOT_SET{ -9999, -9999 };

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
		const Rect2i &free_rect,
		const Region::Size &size,
		Direction::E in_dir
	);
	bool try_add_region_from_search(
		Ref<Region> region, const Region::Size &size, const Direction::E in_dir
	);

	void add_free_edges_to_cache(Ref<Region> region, const Rect2i &region_rect_e);

	void fill_blocked_sides(
		const LocalVector<Region::BlockedSide> &blocked_sides,
		const Rect2i &region_rect
	);
	void fill_corner(
		const LocalVector<Region::BlockedSide> &blocked_sides,
		const Vector2i& corner_gpos,
		Direction::E tile_side
	);

	void try_place_internal(const Region::InternalEntry &choice, const Vector2i &gpos);

	void fill_internal(Ref<Region> region, const Rect2i &region_rect);

	static Rect2i anchored_pos_to_region_rect(
		const Vector2i &anchored_pos_inc,
		const Region::Size &region_size,
		Direction::E in_dir,
		Ref<Region> region
	);

	void debug_region(Ref<Region> region, const Rect2i &region_rect_inc) const;

protected:
	static void _bind_methods();

public:
	static void initialize(Vector2i seg_g_size, bool is_debug = false);
	
	static Ref<Zone> create(
		Ref<RandomNumberGenerator> rng, Ref<PCG> pcg, int max_secondary_count, int w_seg
	);
};
