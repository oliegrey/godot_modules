#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/typed_array.h"

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
		Vector2i gpos_alignment;
		int32_t placement;

		static InternalEntry make_callable(
			const Callable &p_callable,
			const Vector2i p_size,
			const Vector2 p_gpos_alignment,
			const int32_t p_placement
		) {
			InternalEntry e;
			e.type = TYPE_CALLABLE;
			e.callable = p_callable;
			e.size = p_size;
			e.gpos_alignment = Vector2i(p_gpos_alignment);
			e.placement = p_placement;
			return e;
		}

		static InternalEntry make_tile_ref(
			const int32_t p_tile_index,
			const int32_t p_layer_offset,
			const Vector2i p_size,
			const Vector2 p_gpos_alignment,
			const int32_t p_placement
		) {
			InternalEntry e;
			e.type = TYPE_TILE_REF;
			e.tile_index = p_tile_index;
			e.layer_offset = p_layer_offset;
			e.size = p_size;
			e.gpos_alignment = Vector2i(p_gpos_alignment);
			e.placement = p_placement;
			return e;
		}
	};

	struct InternalChoiceSet {
		LocalVector<InternalEntry> choice_set;
		PackedFloat32Array norm_weights;
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
	enum Axis { NO_AXES, X, Y, ALL_AXES };
	enum Placement { RANDOM, CENTER, START, END, FILL, FORCE_GPOS };
	enum BlockedFill { DIRT, STONE, MIX, ANY, ANY_STONE };

	inline static const Vector2i A_NONE{ 0, 0 };
	inline static const Vector2i A_UP{ 0, 1 };
	inline static const Vector2i A_DOWN{ 0, -1 };
	inline static const Vector2i A_LEFT{ 1, 0 };
	inline static const Vector2i A_RIGHT{ -1, 0 };

private:
	using RegionVector = LocalVector<Ref<Region>>;
	using DirEdge = std::array<LocalVector<Edge>, Direction::DIRECTION_MAX>;

	inline static const Vector2i MAX_G_SIZE{ Vector2i(8, 8) };
	inline static const int MAX_CELL_COUNT{ 64 };
	inline static const int FLAT_TREE_SIZE{ MAX_CELL_COUNT * Direction::DIRECTION_MAX };

	inline static Vector2i m_seg_g_size;
	inline static int m_seg_cell_count;

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

	inline static bool is_debug;

	inline static std::array<std::array<uint64_t, 8>, 8> dominance_mask;

public:
	inline static const Vector2i NOT_SET{ -9999, -9999 };

	String name;
	Slot slot;
	Vector2i g_size;

	PackedInt32Array blocked_sides;
	PackedInt32Array blocked_fill;

	PackedInt32Array joining_sides;
	float spawn_weight;
	int threshold;
	Vector2i g_size_inclusive; // includes stone sides
	Vector2i rand_length_addition = NOT_SET;

	LocalVector<InternalChoiceSet> internal_choices;

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

	void add_free_edge_gpos(
		Vector2i gpos, Vector2i rand_g_size, DirEdge &dir_to_free_edge_gpos
	);

	void add_region(
		Ref<RandomNumberGenerator> rng,
		Ref<PCG> pcg,
		Vector2i gpos,
		Vector2i rand_g_size,
		DirEdge &dir_to_free_edge_gpos,
		int w_seg
	);

	bool try_place_s_region(
		Ref<RandomNumberGenerator> rng,
		std::array<uint64_t, Direction::DIRECTION_MAX> &dir_size_occ,
		std::array<PackedVector2Array, FLAT_TREE_SIZE> &dir_size_to_gpos,
		Ref<PCG> pcg,
		DirEdge &dir_to_free_edge_gpos,
		Ref<BitGrid2D> gen_occupancy,
		int w_seg
	);

	static void debug_region(
		Vector2i gpos, Vector2i rand_g_size, Ref<Region> region, int w_seg
	);

	static int get_size_i(Vector2i size);

	static void fill_blocked(
		BlockedFill fill,
		Ref<RandomNumberGenerator> rng,
		Ref<PCG> pcg,
		const Vector2i gpos,
		const Vector2i rect,
		const bool skip_dirt = false
	);

	static void fill_blocked_rect(
		BlockedFill fill,
		Ref<RandomNumberGenerator> rng,
		Ref<PCG> pcg,
		const Vector2i gpos,
		const Vector2i rect
	);

	void fill_blocked_edges(
		Vector2i internal_gpos,
		Vector2i rand_g_size,
		Ref<RandomNumberGenerator> rng,
		Ref<PCG> pcg
	);

	void fill_internal(
		Vector2i internal_gpos,
		Vector2i rand_g_size,
		Ref<RandomNumberGenerator> rng,
		Ref<PCG> pcg
	);

	void try_place_internal(
		InternalEntry choice, Vector2i gpos, Ref<PCG> pcg, Ref<RandomNumberGenerator> rng
	);

protected:
	static void _bind_methods();

public:
	static Vector2i ALIGN_NONE();
	static Vector2i ALIGN_UP();
	static Vector2i ALIGN_DOWN();
	static Vector2i ALIGN_LEFT();
	static Vector2i ALIGN_RIGHT();
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

	static int try_mirror_axis_dir_i(int axis_i, int dir_i);

	static Ref<Region> create(
		String _name,
		Slot _slot,
		Vector2i _g_size,
		int _spawn_weight,
		int _threshold,

		PackedInt32Array _blocked_sides,
		PackedInt32Array _blocked_fill,
		PackedInt32Array _joining_sides,

		TypedArray<Array> internal_class_or_tile_choices, // arrays of [callable, tile_i, ...]
		TypedArray<PackedInt32Array> internal_weights,
		TypedArray<PackedVector2Array> internal_gpos_alignments,
		TypedArray<PackedInt32Array> internal_placements,

		Vector2i _rand_length_addition = Vector2i(0, 0),
		Axis mirror_axes = Axis::NO_AXES
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
VARIANT_ENUM_CAST(Region::Axis)
