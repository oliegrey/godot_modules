#include "draw.h"

#include "modules/tile/tile.h"
#include "modules/bit_grid_2d/bit_grid_2d.h"

void Draw::_bind_methods() {
	ClassDB::bind_static_method(
		"Draw", D_METHOD("initialize", "layers", "grid_size", "_tile_dug_action"),
		&Draw::initialize
	);

	ClassDB::bind_static_method(
		"Draw", D_METHOD("segment", "w_seg", "drawn_indexes", "tile_data", "bitgrid"),
		&Draw::segment
	);
	ClassDB::bind_static_method(
		"Draw", D_METHOD("dig_rect", "w_rect", "dig_power"),
		&Draw::dig_rect
	);

	BIND_ENUM_CONSTANT(SET_DUG);
	BIND_ENUM_CONSTANT(DELETE);
	BIND_ENUM_CONSTANT(PASS);
}

Ref<Draw> Draw::initialize(
	const TypedArray<TileMapLayer> &_layers,
	const Vector2i _grid_size,
	PackedInt32Array _tile_dug_action // default is ADD_DUG
) {
	ERR_FAIL_COND_V_MSG(
		_tile_dug_action.size() != Tile::MAX_TILE, Ref<Draw>(),
		vformat(
			"_tile_dug_action must have one entry for each tile (size %s/%s)",
			_tile_dug_action.size(), Tile::MAX_TILE
		)
	);
	Ref<Draw> draw;
	draw.instantiate();

	ERR_FAIL_COND_V_MSG(_layers.is_empty(), Ref<Draw>(), "layers is empty");
	ERR_FAIL_COND_V_MSG(_grid_size.x <= 0 && _grid_size.y <= 0, Ref<Draw>(), "_grid_size area equals zero");

	for (int i = 0; i < _layers.size(); i++) {
		draw->layers.push_back(Object::cast_to<TileMapLayer>(_layers[i]));
	};
	draw->cell_count = _grid_size.x * _grid_size.y;
	draw->grid_size = _grid_size;
	tile_dug_action.resize(Tile::MAX_TILE);
	for (int i{ 0 }; i < Tile::MAX_TILE; ++i) {
		int dug_action_i{ _tile_dug_action[i] };
		DugAction dug_action{ static_cast<DugAction>(dug_action_i) };
		tile_dug_action[i] = dug_action;
	}
	return draw;
}

void Draw::segment(
	const int64_t w_seg,
	const PackedInt64Array &drawn_indexes,
	const PackedByteArray &tile_data,
	Ref<BitGrid2D> dug_bitgrid
) {
	for (int64_t bits : drawn_indexes) {
		if (bits == -1) { // end reached
			break;
		}

		const int64_t seg_cell_i{ bits & 0xFFFF };
		const int64_t tile_i{ tile_data[seg_cell_i] };
		const int64_t layers_i{ seg_cell_i / cell_count };

		if (dug_bitgrid->is_cell_i_set(seg_cell_i % cell_count)) {
			const DugAction dug_action{ tile_dug_action[tile_i] };

			if (dug_action == DugAction::SET_DUG) {
				static TileMapLayer *background_layer{ layers[Tile::BACKGROUND] };
				static const Vector2i dug_atlas_coords{ Tile::get_tile(Tile::DUG)->get_atlas_coords() }; 
				const Vector2i gpos{
					static_cast<int32_t>((bits >> 16) & 0xFFFF),
					static_cast<int32_t>((bits >> 32) & 0xFFFF)
				};
				background_layer->set_cell(gpos, 0, dug_atlas_coords);
			}

			if (dug_action != DugAction::PASS) {
				continue;
			}
		}

		ERR_FAIL_INDEX(layers_i, layers.size());
		TileMapLayer *layer{ layers[seg_cell_i / cell_count] };

		Ref<Tile> tile{ Tile::get_tile(tile_i) };
		if (tile == Ref<Tile>()) {
			continue;
		}

		const Vector2i gpos{
			static_cast<int32_t>((bits >> 16) & 0xFFFF),
			static_cast<int32_t>((bits >> 32) & 0xFFFF)
		};
		layer->set_cell(gpos, 0, tile->get_atlas_coords());
		try_foreground(tile, gpos);
	}
}

// drawn indexes and tile data stay the same, just visual change
void Draw::dig_rect(const Rect2i &w_rect, const int dig_power) {
	static const Vector2i dug_atlas_coord{
		Tile::get_tile(Tile::DUG)->get_atlas_coords()
	};

	static TileMapLayer *background_layer{ layers[Tile::BACKGROUND] };
	Vector2i rect_end{ w_rect.position + w_rect.size };

	for (int y{ w_rect.position.y }; y < rect_end.y; ++y) {
		for (int x{ w_rect.position.x }; x < rect_end.x; ++x) {
			const Vector2i w_gpos{ Vector2i{ x, y } };

			for (int layer_i{ 0 }; layer_i < Tile::MAX_LAYER; ++layer_i) {
				TileMapLayer *layer{ layers[layer_i] };
				Tile::Layer layer_e{ static_cast<Tile::Layer>(layer_i) };
				const Vector2i atlas_coords{ layer->get_cell_atlas_coords(w_gpos) };
				Ref<Tile> tile{ Tile::get_atlas_coord_tile(layer_e, atlas_coords) };

				if (tile == Ref<Tile>()) {
					continue;
				}

				const int64_t tile_i{ tile->tile };
				const DugAction dug_action{ tile_dug_action[tile_i] };

				if (dug_action == DugAction::PASS || dig_power < tile->hardness) {
					continue;
				}
				else if (dug_action == DugAction::SET_DUG) {
					background_layer->set_cell(w_gpos, 0, dug_atlas_coord);
				}

				if (layer != background_layer) {
					layer->set_cell(w_gpos);
				}
				
				try_foreground(tile, w_gpos);

				if (tile->particle != -1) {
					// do particle effect
				}
			}
		}
	}
}

void Draw::try_foreground(Ref<Tile> base_tile, const Vector2i &w_gpos) {
	Ref<Tile> foreground_tile{ base_tile->get_foreground() };
	if (foreground_tile != Ref<Tile>()) {
		static TileMapLayer *foreground_layer{ layers[Tile::FOREGROUND] };
		Vector2i foreground_atlas_coords{ foreground_tile->get_atlas_coords() };
		foreground_layer->set_cell(w_gpos, 0, foreground_atlas_coords);
	}
}
