#pragma once

#include "core/object/ref_counted.h"
#include "scene/2d/tile_map_layer.h"

class Draw : public RefCounted {
	GDCLASS(Draw, RefCounted);

public:
	Vector2i grid_size;
	int cell_count;
	LocalVector<TileMapLayer *> layers;

protected:
	static void _bind_methods();

public:
	static Ref<Draw> create(
		const TypedArray<TileMapLayer> &_layers, const Vector2i _grid_size
	);

	void segment(
		const int64_t w_seg,
		const PackedInt64Array& drawn_indexes,
		const PackedByteArray& tile_data
	);
};
