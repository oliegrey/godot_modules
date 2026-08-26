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

	struct Size {
		Vector2i i; // includes blocked sides
		int i_cell_count;
		Vector2i e;
		int e_cell_count;

		Size(const Size &size) :
				i{ size.i },
				i_cell_count{ size.i_cell_count },
				e{ size.e },
				e_cell_count{ size.e_cell_count }
			{ }

		Size(const Vector2i &_exclusive, const LocalVector<BlockedSide> &blocked_sides) :
				e{ _exclusive },
				e_cell_count{ _exclusive.x * _exclusive.y } {

			i = _exclusive;
			for (BlockedSide blocked_side : blocked_sides) {
				if (
					blocked_side.direction == Direction::UP ||
					blocked_side.direction == Direction::DOWN
				) {
					i.y += 1;
				} else if (
					blocked_side.direction == Direction::LEFT ||
					blocked_side.direction == Direction::RIGHT
				) {
					i.x += 1;
				}
			}
			i_cell_count = i.x * i.y;
			ERR_FAIL_COND_MSG(
				i.x > MAX_G_SIZE_X, vformat("i.x size(%s) exceeeds max(%s)", i.x, MAX_G_SIZE_X)
			);
			ERR_FAIL_COND_MSG(
				i.y > MAX_G_SIZE_Y, vformat("i.y size(%s) exceeeds max(%s)", i.y, MAX_G_SIZE_Y)
			);
		}

		Size &operator+=(const Vector2i &p_extend) {
			i += p_extend;
			i_cell_count = i.x * i.y;
			e += p_extend;
			e_cell_count = e.x * e.y;
			return *this;
		}
	};

	inline static const LocalVector<Vector2i> s_dir_to_gpos_alignment{
		{ 0, 0 }, { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 }
	};

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
	Size size;
	LocalVector<BlockedSide> blocked_sides;
	LocalVector<Direction::E> free_sides;
	LocalVector<Direction::E> joining_sides;
	float spawn_weight;
	int threshold;
	Vector2i rand_length_addition{ NOT_SET };
	LocalVector<InternalChoiceSet> internal_choices;

protected:
	static void _bind_methods();

public:
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
		PackedInt32Array _blocked_tiles,
		PackedInt32Array _blocked_fill,
		PackedInt32Array _joining_sides,

		TypedArray<Array> internal_class_or_tile_choices,
		TypedArray<PackedInt32Array> internal_weights,
		TypedArray<PackedInt32Array> internal_alignments,
		TypedArray<PackedInt32Array> internal_placements,

		Vector2i _rand_length_addition,
		Axis::E mirror_axes
	);

	static void finalize();

	static float get_slot_weights_sum(const Slot slot, const int threshold_i);

	static int get_slot_threshold_i(const Slot slot, const int gate);

	static Ref<Region> get_slot_rand_region(
		const Slot slot, const int threshold_i, const float weights_sum, const float rand_float
	);

	String get_internal_choices_debug() const;
};

VARIANT_ENUM_CAST(Region::Slot)
VARIANT_ENUM_CAST(Region::Placement)
