#include "pcg.h"

void PCG::_bind_methods() {
	ClassDB::bind_static_method(
		"PCG", D_METHOD("create", "segment_grid_size", "w_seg", "is_server"),
		&PCG::create
	);

	ClassDB::bind_method(D_METHOD("resize_drawn_indexes"), &PCG::resize_drawn_indexes);
	ClassDB::bind_method(D_METHOD("get_generative_occupancy"), &PCG::get_generative_occupancy);
	ClassDB::bind_method(D_METHOD("get_tile_data"), &PCG::get_tile_data);
	ClassDB::bind_method(D_METHOD("get_drawn_indexes"), &PCG::get_drawn_indexes);
	ClassDB::bind_method(D_METHOD("get_drawn_indexes_i"), &PCG::get_drawn_indexes_i);
	ClassDB::bind_method(D_METHOD("get_used_cell_count"), &PCG::get_used_cell_count);
	ClassDB::bind_method(D_METHOD("clear_occupancy"), &PCG::clear_occupancy);

	ClassDB::bind_method(
		D_METHOD(
			"add_cell_i",
			"layer_cell_i",
			"cell_i",
			"tile_i",
			"seg_gpos",
			"add_occupancy"
		),
		&PCG::add_cell_i,
		DEFVAL(true)
	);
	ClassDB::bind_method(
		D_METHOD(
			"add_gpos_tile",
			"layer_offset",
			"tile_i",
			"seg_gpos",
			"add_occupancy"
		),
		&PCG::add_gpos_tile,
		DEFVAL(true)
	);
	ClassDB::bind_method(
		D_METHOD(
			"add_gpos_tiles",
			"layer_offsets",
			"tile_indexes",
			"seg_gpos",
			"add_occupancy"
		),
		&PCG::add_gpos_tiles,
		DEFVAL(true)
	);
	ClassDB::bind_method(
		D_METHOD(
			"add_area",
			"layer_offsets",
			"tile_indexes",
			"seg_gpos",
			"g_size",
			"add_occupancy"
		),
		&PCG::add_area,
		DEFVAL(true)
	);
	ClassDB::bind_method(
		D_METHOD(
			"add_row",
			"layer_offsets",
			"tile_indexes",
			"seg_gpos_y",
			"add_occupancy"
		),
		&PCG::add_row,
		DEFVAL(true)
	);
	ClassDB::bind_method(
		D_METHOD("fill", "tile_i", "set_occupancy", "layer_offset"),
		&PCG::fill,
		DEFVAL(255), DEFVAL(false), DEFVAL(-1)
	);

	ClassDB::bind_method(
		D_METHOD("fill_unoccupied", "tile_i", "set_occupancy", "layer_offset"),
		&PCG::fill_unoccupied,
		DEFVAL(255), DEFVAL(false), DEFVAL(-1)
	);
}

Ref<PCG> PCG::create(
	Vector2i segment_grid_size,
	int w_seg,
	int layer_count,
	bool is_server
) {
	Ref<PCG> pcg;
	pcg.instantiate();
	pcg->m_seg_grid_size = segment_grid_size;
	pcg->m_w_seg_y = w_seg * segment_grid_size.y;
	pcg->m_is_server = is_server;

	pcg->generative_occupancy = BitGrid2D::create(segment_grid_size);
	int cell_count = segment_grid_size.x * segment_grid_size.y;
	pcg->tile_data.resize(layer_count * cell_count);
	pcg->tile_data.fill(255);
	pcg->drawn_indexes.resize(cell_count * layer_count);
	pcg->drawn_indexes.fill(-1);
	return pcg;
}

Vector2i PCG::to_world(Vector2i seg_gpos) {
	seg_gpos.y += m_w_seg_y;
	return seg_gpos;
}

int PCG::get_layer_cell_i(int layer_offset, Vector2i seg_gpos) {
	return layer_offset + seg_gpos.y * m_seg_grid_size.x + seg_gpos.x;
}

int PCG::get_cell_i(Vector2i seg_gpos) {
	return seg_gpos.y * m_seg_grid_size.x + seg_gpos.x;
}

void PCG::add_tile_data(int layer_cell_i, int tile_i) {
	tile_data.set(layer_cell_i, tile_i);
}

void PCG::add_generative_occupancy(int cell_i) {
	generative_occupancy->set_cell_i(cell_i);
	used_cell_count += 1;
}

void PCG::add_drawn_index(int layer_cell_i, Vector2i seg_gpos) {
	Vector2i w_gpos{ to_world(seg_gpos) };
	drawn_indexes.set( // TileMapLayer is 2 byte coordinates
			drawn_indexes_i, layer_cell_i | w_gpos.x << 16 | static_cast<int64_t>(w_gpos.y) << 32
	);
	drawn_indexes_i += 1;
}

void PCG::add_cell_i(
	int layer_cell_i,
	int cell_i,
	int tile_i,
	Vector2i seg_gpos,
	bool add_occupancy
) {
	add_tile_data(layer_cell_i, tile_i);
	if (add_occupancy) { add_generative_occupancy(cell_i); }
	if (!m_is_server) { add_drawn_index(layer_cell_i, seg_gpos); }
}

void PCG::add_gpos_tile(
	int layer_offset, int tile_i, Vector2i seg_gpos, bool add_occupancy
) {
	int cell_i{ get_cell_i(seg_gpos) };
	add_cell_i(layer_offset + cell_i, cell_i, tile_i, seg_gpos, add_occupancy);
}

void PCG::add_gpos_tiles(
	PackedInt32Array layer_offsets,
	PackedInt32Array tile_indexes,
	Vector2i seg_gpos,
	bool add_occupancy
) {
	for (int i{ 0 }; i < tile_indexes.size(); ++i) {
		add_gpos_tile(
			layer_offsets[i], tile_indexes[i], seg_gpos, add_occupancy
		);
	}
}

void PCG::add_area(
	PackedInt32Array layer_offsets,
	PackedInt32Array tile_indexes,
	Vector2i seg_gpos,
	Vector2i g_size,
	bool add_occupancy
) {
	for (int i{ 0 }; i < tile_indexes.size(); ++i) {
		int tile_i{ tile_indexes[i] };
		int layer_offset{ layer_offsets[i] };

		for (int y{ 0 }; y < g_size.y; ++y) {
			for (int x{ 0 }; x < g_size.x; ++x) {
				const Vector2i gpos{ seg_gpos + Vector2i(x, y) };
				add_gpos_tile(layer_offset, tile_i, gpos, add_occupancy);
			}
		}
	}
}

void PCG::add_row(
	PackedInt32Array layer_offsets,
	PackedInt32Array tile_indexes,
	int seg_gpos_y,
	bool add_occupancy
) {
	for (int i{ 0 }; i < layer_offsets.size(); ++i) {
		int tile_i{ tile_indexes[i] };
		int layer_offset{ layer_offsets[i] };

		for (int seg_gpos_x{ 0 }; seg_gpos_x < m_seg_grid_size.x; ++seg_gpos_x) {
			const Vector2i gpos{ Vector2i(seg_gpos_x, seg_gpos_y) };
			add_gpos_tile(layer_offset, tile_i, gpos, add_occupancy);
		}
	}
}

void PCG::fill(int tile_i, bool add_occupancy, int layer_offset) {
	tile_data.fill(tile_i);
	if (add_occupancy) { generative_occupancy->fill(); }
	if (!m_is_server) {
		ERR_FAIL_COND_MSG(layer_offset < 0, "layer_offset is required for client");
		for (int x{ 0 }; x < m_seg_grid_size.x; ++x) {
			for (int y{ 0 }; y < m_seg_grid_size.y; ++y) {
				Vector2i gpos{ Vector2i(x, y) };
				int layer_cell_i{ get_layer_cell_i(layer_offset, gpos) };
				add_drawn_index(layer_cell_i, gpos);
			}
		}
	}
}

void PCG::fill_unoccupied(int tile_i, bool add_occupancy, int layer_offset) {
	for (int x{ 0 }; x < m_seg_grid_size.x; ++x) {
		for (int y{ 0 }; y < m_seg_grid_size.y; ++y) {
			Vector2i gpos{ Vector2i(x, y) };
			if (!generative_occupancy->is_gpos_set(gpos)) {
				add_gpos_tile(layer_offset, tile_i, gpos, add_occupancy);
			}
		}
	}
}
