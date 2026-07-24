#include "draw.h"

#include "modules/tile/tile.h"

void Draw::_bind_methods() {
	ClassDB::bind_static_method(
		"Draw", D_METHOD("create", "layers", "grid_size"),
		&Draw::create
	);

	ClassDB::bind_method(
		D_METHOD("segment", "w_seg", "drawn_indexes", "tile_data"),
		&Draw::segment
	);
}

Ref<Draw> Draw::create(
	const TypedArray<TileMapLayer> &_layers, const Vector2i _grid_size
) {
	Ref<Draw> draw;
	draw.instantiate();

	ERR_FAIL_COND_V_MSG(_layers.is_empty(), Ref<Draw>(), "layers is empty");
	ERR_FAIL_COND_V_MSG(_grid_size.x <= 0 && _grid_size.y <= 0, Ref<Draw>(), "_grid_size area equals zero");

	for (int i = 0; i < _layers.size(); i++) {
		draw->layers.push_back(Object::cast_to<TileMapLayer>(_layers[i]));
	};
	draw->cell_count = _grid_size.x * _grid_size.y;
	draw->grid_size = _grid_size;
	return draw;
}

void Draw::segment(
	const int64_t w_seg,
	const PackedInt64Array &drawn_indexes,
	const PackedByteArray &tile_data
) {
	for (int64_t bits : drawn_indexes) {

		const int64_t seg_cell_i{ bits & 0xFFFF };
		TileMapLayer *layer{ layers[seg_cell_i / cell_count] };

		const int64_t tile_i{ tile_data[seg_cell_i] };
		Ref<Tile> tile{ Tile::get_tile(tile_i) };

		layer->set_cell(
			Vector2i((bits >> 16) & 0xFFFF, (bits >> 32) & 0xFFFF),
			0, tile->get_atlas_coords()
		);
	}
}
