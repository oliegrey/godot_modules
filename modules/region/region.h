#pragma once

#include "core/object/ref_counted.h"

#include <array>

class Region;
class RandomNumberGenerator;
class PCG;
class BitGrid2D;

class Region : public RefCounted {
	GDCLASS(Region, RefCounted);

public:
	struct Edge { Vector2i gpos; Vector2i size; };

	struct InternalEntry {
		enum Type { TYPE_CALLABLE, TYPE_TILE_REF };

		Type type;
		Callable callable;
		int32_t tile_index = -1;
		int32_t layer_offset = -1;

		Vector2i size;
		int32_t weight;

		static InternalEntry make_callable(
			const Callable &p_callable,
			const Vector2i p_size,
			const int32_t p_weight
		) {
			InternalEntry e;
			e.type = TYPE_CALLABLE;
			e.callable = p_callable;
			e.size = p_size;
			e.weight = p_weight;
			return e;
		}

		static InternalEntry make_tile_ref(
			const int32_t p_tile_index,
			const int32_t p_layer_offset,
			const Vector2i p_size,
			const int32_t p_weight
		) {
			InternalEntry e;
			e.type = TYPE_TILE_REF;
			e.tile_index = p_tile_index;
			e.layer_offset = p_layer_offset;
			e.size = p_size;
			e.weight = p_weight;
			return e;
		}
	};

	struct Internal {
		LocalVector<InternalEntry> entries;
		int32_t anchor_dir = 0;
	};

	enum Slot { PRIMARY, SECONDARY };
	enum Direction {
		NONE = -1,
		UP = 0,
		DOWN = 1,
		LEFT = 2,
		RIGHT = 3,
		DIRECTION_MAX = 4
	};

	enum Placement { FILL = -1, RANDOM = -2 };
	enum BlockedFill { DIRT, STONE, MIX, ANY, ANY_STONE };

private:
	using RegionVector = LocalVector<Ref<Region>>;
	using DirEdge = std::array<LocalVector<Edge>, Direction::DIRECTION_MAX>;

	inline static const Vector2i MAX_G_SIZE{ Vector2i(8, 8) };
	inline static const int MAX_CELL_COUNT{ 64 };
	inline static const int FLAT_TREE_SIZE{ MAX_CELL_COUNT * Direction::DIRECTION_MAX };

	inline static Vector2i m_seg_g_size;
	inline static int m_seg_cell_count;

	// index = attachment Direction [0 - 3] * quad size flattened i.e. (4 * 3)
	// -> all secondary regions that fit size and direction
	// RegionVector ordered by threshold so its easy to iterate within bounds
	inline static std::array<RegionVector, FLAT_TREE_SIZE> m_region_tree{};
	// bitmap to find the next size that has matching regions
	inline static std::array<uint64_t, Direction::DIRECTION_MAX> m_region_tree_occ{};

	// attachment direction (already placed regions perspective) [0 - 3] -> region
	// to get a random region to test in a free direction
	// RegionVector ordered by threshold so its easy to iterate within bounds
	inline static std::array<RegionVector, Direction::DIRECTION_MAX> m_dir_to_region{};

	// starting regions to build off of
	// ordered by threshold so its easy to iterate within bounds
	inline static RegionVector m_primary_regions{};
	inline static PackedFloat32Array m_primary_weights{};
	inline static float m_primary_weight_sum{};
	// ordered by threshold so its easy to iterate within bounds
	inline static RegionVector m_secondary_regions{};
	inline static PackedFloat32Array m_secondary_weights{};

	int m_cell_count;

	inline static bool is_debug;

	inline static std::array<std::array<uint64_t, 8>, 8> dominance_mask;

public:
	String name;
	Slot slot;
	Vector2i g_size;

	PackedInt32Array blocked_sides;
	PackedInt32Array blocked_fill;

	PackedInt32Array joining_sides;
	float spawn_weight;
	int threshold;
	Vector2i g_size_inclusive; // includes stone sides

	LocalVector<Internal> internal_choices;

private:
	static Direction invert_direction(Direction direction) {
		switch (direction) {
			case Direction::UP:       return Direction::DOWN;
			case Direction::DOWN:     return Direction::UP;
			case Direction::LEFT:     return Direction::RIGHT;
			case Direction::RIGHT:    return Direction::LEFT;
			default:                  return Direction::NONE;
		}
	}

	static void init_dominance_mask();

	static int get_size_or_larger_i(uint64_t bitmap, const Vector2i size);

	static void add_free_edge_gpos(
		Ref<Region> region, Vector2i gpos, DirEdge &dir_to_free_edge_gpos
	);

	static void add_region(
		Ref<Region> region,
		Ref<PCG> pcg,
		Vector2i gpos,
		DirEdge &dir_to_free_edge_gpos,
		int w_seg
	);

	static bool try_place_s_region(
		Ref<RandomNumberGenerator> rng,
		Ref<Region> s_region,
		std::array<uint64_t, Direction::DIRECTION_MAX> &dir_size_occ,
		std::array<PackedVector2Array, FLAT_TREE_SIZE> &dir_size_to_gpos,
		Ref<PCG> pcg,
		DirEdge &dir_to_free_edge_gpos,
		Ref<BitGrid2D> gen_occupancy,
		int w_seg
	);

	static void debug_region(Vector2i gpos, Ref<Region> region, int w_seg);

	static int get_size_i(Vector2i size);

protected:
	static void _bind_methods();

public:
	String get_name() const;
	Region::Slot get_slot() const;
	Vector2i get_g_size() const;
	PackedInt32Array get_blocked_sides() const;
	PackedInt32Array get_joining_sides() const;
	float get_spawn_weight() const;
	int get_threshold() const;
	Vector2i get_g_size_inclusive() const;
	PackedInt32Array get_blocked_fill() const;

	static void initialize(Vector2i seg_g_size, bool debug = false);

	static Ref<Region> create(
		String _name,
		Slot _slot,
		Vector2i _g_size,

		PackedInt32Array _blocked_sides,
		PackedInt32Array _blocked_fill,

		PackedInt32Array _joining_sides,

		TypedArray<Array> internal_callable_or_tile_choices,
		TypedArray<PackedInt32Array> internal_weights,
		PackedInt32Array internal_anchor_dir,

		int _spawn_weight,
		int _threshold
	);

	static void finalize();

	static void generate_zone(
		Ref<RandomNumberGenerator> rng,
		Ref<PCG> pcg,
		const int w_seg,
		const int max_secondary_count
	);

	static float get_weight_sum_bounded(
		const PackedFloat32Array &p_weights, const int exl_upper_bound
	);

	static int rand_weighted_bound(
		const Ref<RandomNumberGenerator> rng,
		const PackedFloat32Array &p_weights,
		const int exl_upper_bound,
		const float weights_sum
	);

	String get_internal_choices_debug() const;
};

VARIANT_ENUM_CAST(Region::Slot)
VARIANT_ENUM_CAST(Region::Direction)
VARIANT_ENUM_CAST(Region::Placement)
VARIANT_ENUM_CAST(Region::BlockedFill)
