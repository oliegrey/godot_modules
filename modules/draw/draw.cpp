#include "draw.h"

void Draw::_bind_methods() {
	ClassDB::bind_method(
		D_METHOD("segment", "w_seg", "drawn_indexes", "tile_data"),
		&Draw::segment
	);

	ClassDB::bind_method(
		D_METHOD("setup", "layers", "layer_configs", "grid_size"),
		&Draw::setup
	);
}

void Draw::setup(
	const TypedArray<TileMapLayer> &_layers,
	const TypedArray<Array> &_layer_configs,
	const Vector2i _grid_size
) {
	for (int i = 0; i < _layers.size(); i++) {
		layers.push_back(Object::cast_to<TileMapLayer>(_layers[i]));
	};
	layer_configs = _layer_configs;
	cell_count = _grid_size.x * _grid_size.y;
	grid_size = _grid_size;
}

void Draw::segment(
		const int64_t w_seg,
		const PackedInt64Array &drawn_indexes,
		const PackedByteArray &tile_data)
{
	ERR_FAIL_COND_MSG(layers.is_empty(), "setup likely not completed");
	int64_t layer_i{ -1 };
	Array layer_config{};
	TileMapLayer *layer{ nullptr };

	for (int64_t bits : drawn_indexes) {
		const int64_t seg_cell_i{ bits & 0xFFFF };
		const int64_t this_layer_i{ seg_cell_i / cell_count };

		if (layer_i != this_layer_i) {
			layer_i = this_layer_i;
			layer_config = layer_configs[layer_i];
			layer = layers[layer_i];
		}
		const int64_t tile_i{ tile_data[seg_cell_i] };

		Object *tile{ Object::cast_to<Object>(layer_config[tile_i].operator Object *()) };

		const Variant sa{ tile->get("state_atlas_coords") };
		const Array state_atlas{ sa };
		const Vector2i atlas_coords{ state_atlas[0] };

		layer->set_cell(
				Vector2i((bits >> 16) & 0xFFFF, (bits >> 32) & 0xFFFF),
				0,
				atlas_coords);
	}
}

