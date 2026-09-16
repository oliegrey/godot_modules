#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/typed_array.h"
#include "modules/pcg/pcg.h"
#include "modules/direction/direction.h"
#include "modules/axis/axis.h"
#include "modules/tile/tile.h"

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
	public:
		enum Type { TYPE_CALLABLE, TYPE_TILE_REF };

	public:
		inline static const LocalVector<Vector2i> s_dir_to_gpos_alignment{
			{ 0, 0 }, { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 }
		};

		Type type;
		Callable callable;
		Ref<Tile> tile;
		Vector2i size;
		Vector2i gpos_alignment;
		Direction::E alignment;
		int32_t placement;

	public:
		static InternalEntry make_callable(
			const Callable &_callable,
			const Vector2i &_size,
			const Variant _alignment,
			const int32_t _placement
		) {
			InternalEntry e;
			e.type = TYPE_CALLABLE;
			e.callable = _callable;
			e.size = _size;
			init_alignment(e, _alignment);
			e.placement = _placement;
			return e;
		}

		static InternalEntry make_tile_ref(
			Ref<Tile> tile, const Variant _alignment, const int32_t p_placement
		) {
			InternalEntry e;
			e.type = TYPE_TILE_REF;
			e.tile = tile;
			e.size = tile.is_valid() ? tile->g_size : Vector2i();
			init_alignment(e, _alignment);
			e.placement = p_placement;
			return e;
		}

		static void init_alignment(InternalEntry &e, const Variant _alignment) {
			if (_alignment.get_type() == Variant::INT) {
				const int alignment_i{ _alignment };
				Direction::E alignment{ static_cast<Direction::E>(alignment_i) };
				e.alignment = alignment;
				if (alignment != Direction::NONE && alignment != Direction::MAX) {
					e.gpos_alignment = Vector2i(s_dir_to_gpos_alignment[alignment]);
				}

			} else if (_alignment.get_type() == Variant::VECTOR2I) {
				e.alignment = Direction::NONE;
				e.gpos_alignment = _alignment;

			} else {
				ERR_FAIL_MSG(vformat("passed alignment type (%s) is incompatible", _alignment.get_type()));
			}
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

		BlockedSide() = default;

		BlockedSide(Direction::E _direction, PCG::Fill _fill, LocalVector<Ref<Tile>> _tiles)
		: direction{ _direction }, fill{ _fill }, tiles{ _tiles } {}

		BlockedSide(Direction::E _direction, PCG::Fill _fill, PackedInt32Array _tiles)
		: direction{ _direction }, fill{ _fill } {
			tiles.resize(_tiles.size());
			for (int i{ 0 }; i < _tiles.size(); ++i) {
				const int tile_i{ _tiles[i] };
				ERR_FAIL_INDEX(tile_i, Tile::MAX_TILE);
				tiles[i] = Tile::get_tile(tile_i);
			}
		}
	};

	struct Size {
		Vector2i i; // includes blocked sides
		int i_cell_count;
		Vector2i e;
		int e_cell_count;

		Size() = default;

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
	Vector2i get_size_e() const;
	Vector2i get_size_i() const;
	float get_spawn_weight() const;
	int get_threshold() const;

	static void initialize(Vector2i seg_g_size, bool debug = false);

	static Ref<Region> create(
		String _name,
		Slot _slot,
		Vector2i _g_size,
		int _spawn_weight,
		int _threshold,

		PackedInt32Array _blocked_sides,
		TypedArray<PackedInt32Array> _blocked_tile_sets,
		PackedInt32Array _blocked_fill,
		PackedInt32Array _joining_sides,

		TypedArray<Array> internal_callable_or_tile_choices,
		TypedArray<PackedInt32Array> internal_weights,
		TypedArray<Array> internal_alignments,
		TypedArray<PackedInt32Array> internal_placements,

		Vector2i _rand_length_addition = Vector2i(0, 0),
		Axis::E mirror_axes = Axis::NONE
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
