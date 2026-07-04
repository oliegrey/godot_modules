#pragma once

#include "core/object/ref_counted.h"
#include "modules/bit_grid_2d/bit_grid_2d.h"

class SubgridProbe;

class PCG : public RefCounted {
	GDCLASS(PCG, RefCounted);

private:
	Vector2i m_seg_grid_size;
	int cell_count;
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
		Vector2i m_segment_grid_size,
		int w_seg,
		int layer_count,
		bool is_server
	);

	Ref<BitGrid2D> get_generative_occupancy() const { return generative_occupancy; }
	const PackedByteArray& get_tile_data() const { return tile_data; }
	PackedInt64Array get_drawn_indexes() const {
		return drawn_indexes.slice(0, drawn_indexes_i); // should only be called once or twice, slicing fine
	}
	int get_drawn_indexes_i() const { return drawn_indexes_i; }
	int get_used_cell_count() const { return used_cell_count; }

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
		PackedInt32Array layer_offsets,
		PackedInt32Array tile_indexes,
		Vector2i seg_gpos,
		bool add_occupancy = true
	);

	void add_tile_rect(
		int layer_offset,
		int tile_i,
		Vector2i seg_gpos,
		Vector2i g_size,
		bool add_occupancy
	);
	void add_tiles_rect(
		PackedInt32Array layer_offsets,
		PackedInt32Array tile_indexes,
		Vector2i seg_gpos,
		Vector2i g_size,
		bool add_occupancy = true
	);
	void add_tile_ellipse(
		int layer_offset,
		int tile_i,
		Vector2i seg_gpos,
		Vector2i g_size,
		bool add_occupancy = true
	);
	void add_tiles_ellipse(
		PackedInt32Array layer_offsets,
		PackedInt32Array tile_indexes,
		Vector2i seg_gpos,
		Vector2i g_size,
		bool add_occupancy = true
	);
	int add_rand_agnostic_ellipse(
		Ref<RandomNumberGenerator> rng,
		int layer_offset,
		int tile_i,
		Vector2i g_size,
		int count = 1,
		Ref<SubgridProbe> advance_bucket = Ref<SubgridProbe>(),
		bool add_occupancy = true
	);
	void add_rand(
		Ref<RandomNumberGenerator> rng,
		int layer_offset,
		int tile_i,
		int count = 1,
		Ref<SubgridProbe> advance_bucket = Ref<SubgridProbe>(),
		bool add_occupancy = true
	);

	void add_row(
		PackedInt32Array layer_offsets,
		PackedInt32Array tile_indexes,
		int seg_gpos_y,
		bool add_occupancy = true
	);

	void fill(
		int tile_i = 255, bool add_occupancy = false, int layer_offset = -1
	);

	void fill_unoccupied(
		int tile_i = 255, bool add_occupancy = false, int layer_offset = -1
	);

	void clear_occupancy() const { generative_occupancy->clear(); }

	Vector2i get_random_gpos(
		Vector2i g_size,
		Vector2i x_gpos_range,
		Vector2i y_gpos_range,
		const PackedByteArray &occupancy_bitmap
	) {
		ERR_FAIL_COND_V_MSG(x_gpos_range.x < 0, Vector2i(), "x min too small");
		ERR_FAIL_COND_V_MSG(y_gpos_range.x < 0, Vector2i(), "y min too small");
		ERR_FAIL_COND_V_MSG(
			x_gpos_range.y + g_size.x > m_seg_grid_size.x, Vector2i(),
			"the provided x max would be out of bounds"
		);
		ERR_FAIL_COND_V_MSG(
			y_gpos_range.y + g_size.y > m_seg_grid_size.y, Vector2i(),
			"the provided y max would be out of bounds"
		);

		if (occupancy_bitmap.size() >= 0) {

		}
	}
};
