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
	inline static constexpr int FLAT_DIR_CELLS{ Region::MAX_CELL_COUNT * Direction::MAX };

	struct Edge { Vector2i gpos; Vector2i size; };

private:
	using DirEdge = std::array<LocalVector<Edge>, Direction::MAX>;
	using FlatDirSizeToGposArr = std::array<LocalVector<Vector2i>, FLAT_DIR_CELLS>;

	inline static std::array<std::array<uint64_t, 8>, 8> s_dominance_mask;
	inline static Vector2i s_seg_g_size;
	inline static int s_seg_cell_count;
	inline static bool s_is_debug;

	Ref<RandomNumberGenerator> m_rng;
	Ref<PCG> m_pcg;
	Ref<BitGrid2D> m_occ;
	int m_max_secondary_count;
	int m_w_seg;

	DirEdge dir_to_free_edge_gpos; // free grid position look up based on direction requirement; dir -> [free edge gpos, g_size, ...]

private:
	static void init_dominance_mask();

	static int get_size_or_larger_i(uint64_t bitmap, const Vector2i size);
	static int get_size_or_larger_i(uint64_t bitmap, const int cell_count);
	static int get_region_relative_size_i(Vector2i size);

	int rand_weighted_bound(
		const PackedFloat32Array &p_weights,
		const int exl_upper_bound,
		const float weights_sum
	);

	void generate();

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
		Ref<RandomNumberGenerator> rng,
		const LocalVector<Region::BlockedSide> &blocked_sides,
		const Vector2i& corner_gpos
	);

	bool try_add_region_to_edge(
		Direction::E dir,
		Vector2i edge_gpos,
		Vector2i required_size,
		Ref<RandomNumberGenerator> rng,
		Ref<BitGrid2D> gen_occupancy
	);

	void try_place_internal(
		Region::InternalEntry choice,
		Vector2i gpos,
		Ref<PCG> pcg,
		Ref<RandomNumberGenerator> rng
	);

	void add_dir_size_to_gpos(
		std::array<uint64_t, Direction::MAX> &dir_size_occ,
		FlatDirSizeToGposArr &dir_size_to_gpos,
		const int req_dir_offset,
		Direction req_dir,
		const LocalVector<Rect2i> &areas,
		int start_i
	);

	void fill_internal(
		Vector2i internal_gpos,
		Vector2i rand_g_size,
		Ref<RandomNumberGenerator> rng,
		Ref<PCG> pcg
	);

	void add_free_edge_gpos(
		Vector2i gpos, Vector2i rand_g_size, DirEdge &dir_to_free_edge_gpos
	);

	void add_region(
		Ref<RandomNumberGenerator> rng,
		Ref<PCG> pcg,
		Vector2i external_gpos,
		Vector2i _exclusive_g_size,
		DirEdge &dir_to_free_edge_gpos
	);

	bool try_place_s_region(
		Ref<RandomNumberGenerator> rng,
		std::array<uint64_t, Direction::MAX> &dir_size_occ,
		FlatDirSizeToGposArr &dir_size_to_gpos,
		Ref<PCG> pcg,
		DirEdge &dir_to_free_edge_gpos,
		Ref<BitGrid2D> gen_occupancy,
		int w_seg
	);

protected:
	void _bind_methods();

public:
	void initialize(Vector2i seg_g_size, bool is_debug = false);
	
	Ref<Zone> create(
		Ref<RandomNumberGenerator> rng, Ref<PCG> pcg, int max_secondary_count, int w_seg
	);
};
