#pragma once

#include "core/object/ref_counted.h"
#include "scene/2d/tile_map_layer.h"

class BitGrid2D;
class Tile;

class Draw : public RefCounted {
	GDCLASS(Draw, RefCounted);

public:
	enum DugAction { SET_DUG, DELETE, PASS };

private:
	inline static LocalVector<DugAction> tile_dug_action;
	inline static LocalVector<TileMapLayer*> layers;
	inline static Vector2i grid_size;
	inline static int cell_count;

protected:
	static void _bind_methods();

public:
	static Ref<Draw> initialize(
		const TypedArray<TileMapLayer> &_layers,
		const Vector2i _grid_size,
		PackedInt32Array _tile_dug_action
	);

	static void segment(
		const int64_t w_seg,
		const PackedInt64Array &drawn_indexes,
		const PackedByteArray &tile_data,
		Ref<BitGrid2D> bitgrid
	);

	static void dig_rect(const Rect2i &w_rect, const int dig_power);

	static void try_foreground(Ref<Tile> base_tile, const Vector2i &w_gpos);
};

VARIANT_ENUM_CAST(Draw::DugAction);
