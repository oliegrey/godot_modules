#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/typed_array.h"
#include "modules/pcg/pcg.h"
#include "modules/direction/direction.h"
#include "modules/axis/axis.h"
#include "modules/tile/Tile.h"

#include <array>

class Region;
class RandomNumberGenerator;
class BitGrid2D;

class Region : public RefCounted {
	GDCLASS(Region, RefCounted);

public:
	inline static constexpr int MAX_G_SIZE_X{ 8 };
	inline static constexpr int MAX_G_SIZE_Y{ 8 };
	inline static constexpr int MAX_CELL_COUNT{ 64 }; // occupancy bitmap size alias

	enum Slot { PRIMARY, SECONDARY };
	enum Placement { RANDOM, CENTER, START, END, FILL, FORCE_GPOS };

	struct InternalEntry {
		enum Type { TYPE_CALLABLE, TYPE_TILE_REF };

		Type type;
		Callable callable;
		Ref<Tile> tile;
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
			Ref<Tile> tile,
			const Vector2 p_gpos_alignment,
			const int32_t p_placement
		) {
			InternalEntry e;
			e.type = TYPE_TILE_REF;
			e.tile = tile;
			e.size = tile->g_size;
			e.gpos_alignment = Vector2i(p_gpos_alignment);
			e.placement = p_placement;
			return e;
		}
	};

	struct InternalChoiceSet {
		LocalVector<InternalEntry> choice_set;
		PackedFloat32Array norm_weights;
	};

	struct BlockedSide {
		Direction::E direction;
		PCG::Fill fill;
		LocalVector<Ref<Tile>> tiles;

		BlockedSide(Direction::E _direction, PCG::Fill _fill, LocalVector<Ref<Tile>> _tiles)
		: direction{ _direction }, fill{ _fill }, tiles{ _tiles } {}

		BlockedSide(Direction::E _direction, PCG::Fill _fill, PackedInt32Array _tiles)
		: direction{ _direction }, fill{ _fill } {
			for (int i{ 0 }; i < _tiles.size(); ++i) {
				const int tile_i{ _tiles[i] };
				tiles[i] = Tile::get_tile(tile_i);
			}
		}
	};

	inline static const Vector2i A_NONE{ 0, 0 };
	inline static const Vector2i A_UP{ 0, 1 };
	inline static const Vector2i A_DOWN{ 0, -1 };
	inline static const Vector2i A_LEFT{ 1, 0 };
	inline static const Vector2i A_RIGHT{ -1, 0 };

private:
	using RegionVector = LocalVector<Ref<Region>>;

	// attachment direction (already placed regions perspective) [0 - 3] -> region
	// to get a random region to test in a free direction
	// RegionVector ordered by threshold so its easy to iterate within bounds
	inline static std::array<RegionVector, Direction::MAX> m_dir_to_region{};

public:
	inline static const Vector2i NOT_SET{ -9999, -9999 };
	inline static RegionVector primary_regions{}; // ordered by threshold for ease of iteration
	inline static PackedFloat32Array primary_weights{};
	inline static float primary_weight_sum{};
	inline static RegionVector secondary_regions{}; // ordered by threshold for ease of iteration
	inline static PackedFloat32Array secondary_weights{};

	String name;
	Slot slot;
	Vector2i g_size;
	Vector2i g_size_inclusive; // includes stone sides
	LocalVector<BlockedSide> blocked_sides;
	LocalVector<Direction::E> joining_sides;
	float spawn_weight;
	int threshold;
	Vector2i rand_length_addition{ NOT_SET };
	LocalVector<InternalChoiceSet> internal_choices;

private:
	static void debug_region(
		Vector2i gpos, Vector2i rand_g_size, Ref<Region> region, int w_seg
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
	float get_spawn_weight() const;
	int get_threshold() const;
	Vector2i get_g_size_inclusive() const;

	static void initialize(Vector2i seg_g_size, bool debug = false);

	static Ref<Region> create(
		String _name,
		Slot _slot,
		Vector2i _g_size,
		int _spawn_weight,
		int _threshold,

		PackedInt32Array _blocked_sides,
		PackedInt32Array _blocked_fill,
		PackedInt32Array _blocked_tiles,
		PackedInt32Array _joining_sides,

		TypedArray<Array> internal_class_or_tile_choices, // arrays of [callable, tile_i, ...]
		TypedArray<PackedInt32Array> internal_weights,
		TypedArray<PackedVector2Array> internal_gpos_alignments,
		TypedArray<PackedInt32Array> internal_placements,

		Vector2i _rand_length_addition = Vector2i(0, 0),
		Axis::E mirror_axes = Axis::NONE
	);

	static void finalize();

	static float get_weight_sum_bounded(
		const PackedFloat32Array &p_weights, const int exl_upper_bound
	);

	String get_internal_choices_debug() const;
};

VARIANT_ENUM_CAST(Region::Slot)
VARIANT_ENUM_CAST(Region::Placement)
