#pragma once

#include "core/object/ref_counted.h"
#include "modules/bit_grid_2d/bit_grid_2d.h"

class PCG : public RefCounted {
	GDCLASS(PCG, RefCounted);

private:
	Vector2i m_seg_grid_size;
	int m_w_seg_y;
	bool m_is_server;

public:
	Ref<BitGrid2D> generative_occupancy;
	PackedByteArray tile_data;
	PackedInt64Array drawn_indexes;
	int drawn_indexes_i{ 0 };
	int used_cell_count{ 0 };

private:
	Vector2i to_world(Vector2i seg_gpos);
	int get_layer_cell_i(int layer_offset, Vector2i seg_gpos);
	int get_cell_i(Vector2i seg_gpos);

	void add_tile_data(int layer_cell_i, int tile_i);
	void add_generative_occupancy(int cell_i);
	void add_drawn_index(int layer_cell_i, Vector2i seg_gpos);

protected:
	static void _bind_methods();

public:
	static Ref<PCG> create(
		Vector2i m_segment_grid_size, int w_seg, bool is_server
	);

	void add_cell_i(
		int layer_cell_i,
		int cell_i,
		int tile_i,
		Vector2i seg_gpos,
		bool add_occupancy = true
	);

	void add_gpos_tile(
		int layer_offset,
		int tile_i,
		Vector2i seg_gpos,
		bool add_occupancy = true
	);
	void add_gpos_tiles(
		PackedInt32Array tile_indexes,
		PackedInt32Array layer_offsets,
		Vector2i seg_gpos,
		bool add_occupancy = true
	);

	void add_area(
		PackedInt32Array tile_indexes,
		PackedInt32Array layer_offsets,
		Vector2i seg_gpos,
		Vector2i g_size,
		bool add_occupancy = true
	);
	
	void add_row(
		PackedInt32Array layer_offsets,
		PackedInt32Array tile_indexes,
		int seg_gpos_y,
		bool add_occupancy = true
	);

	void fill(
		int tile_i = 255, int set_occupancy = false, int layer_offset = -1
	);
};
