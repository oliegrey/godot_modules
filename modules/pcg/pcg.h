#pragma once

#include "core/object/ref_counted.h"
#include "modules/subgrid_probe/subgrid_probe.h"
#include "modules/tile/tile.h"

class BitGrid2D;
class RandomNumberGenerator;

class PCG : public RefCounted {
	GDCLASS(PCG, RefCounted);

public:
	enum Fill { PICK_ONE, MIX };

private:
	inline static Vector2i m_seg_grid_size;
	inline static int m_cell_count;
	inline static bool m_is_server;

	int m_w_seg_y;

public:
	Ref<BitGrid2D> generative_occupancy;
	PackedByteArray tile_data;
	PackedByteArray anchor_dist_data; // to find the anchor of larger tiles
	PackedInt64Array drawn_indexes; // client only
	int drawn_indexes_i{ 0 };
	int used_cell_count{ 0 };

private:
	static int get_layer_offset(int layer_i);

	Vector2i to_world(Vector2i seg_gpos);
	int get_layer_cell_i(int layer_offset, Vector2i seg_gpos);
	int get_cell_i(Vector2i seg_gpos);

	void add_tile_data(int layer_cell_i, int tile_i);
	void add_generative_occupancy(int cell_i);
	void add_drawn_index(int layer_cell_i, Vector2i seg_gpos);

	uint8_t get_anchor_dist_bits(Vector2i dist) const;
	void add_anchor_dist(int layer_cell_i, Vector2i dist);
	int get_anchor_cell(int layer_cell_i) const;

protected:
	static void _bind_methods();

public:
	static void initialize(Vector2i segment_grid_size, bool is_server);
	static Ref<PCG> create(int w_seg);

	Ref<BitGrid2D> get_generative_occupancy() const;
	PackedByteArray get_anchor_dist_data() const;
	const PackedByteArray &get_tile_data() const { return tile_data; }
	PackedInt64Array get_drawn_indexes() const {
		return drawn_indexes.slice(0, drawn_indexes_i); // should only be called once or twice per segment on the client, slice copy is fine
	}
	int get_drawn_indexes_i() const { return drawn_indexes_i; }
	int get_used_cell_count() const { return used_cell_count; }

	void add_cell_i(
		int layer_cell_i,
		int cell_i,
		int tile_i,
		const Vector2i &seg_gpos,
		bool add_occupancy = true,
		const Vector2i &anchor_dist = Vector2i()
	);

	void add_gpos_tile(
		int layer_i,
		int tile_i,
		const Vector2i &seg_gpos,
		bool add_occupancy = true,
		Ref<RandomNumberGenerator> tile_variation_rng = Ref<RandomNumberGenerator>()
	);

	// internal: takes a precomputed layer offset, not a layer index
	void _add_gpos_tile(
		int layer_offset,
		int tile_i,
		const Vector2i &seg_gpos,
		bool add_occupancy,
		Ref<RandomNumberGenerator> tile_variation_rng
	);

	void add_gpos_tiles(
		const PackedInt32Array &layer_indexes,
		const PackedInt32Array &tile_indexes,
		const Vector2i &seg_gpos,
		bool add_occupancy = true,
		Ref<RandomNumberGenerator> tile_variation_rng = Ref<RandomNumberGenerator>()
	);

	void add_tile_rect(
		int layer_i,
		int tile_i,
		const Rect2i &rect,
		bool add_occupancy = true,
		Ref<RandomNumberGenerator> variation_rng = Ref<RandomNumberGenerator>()
	);
	void add_tiles_rect(
		const PackedInt32Array &layers_i,
		const PackedInt32Array &tiles_i,
		const Rect2i &rect,
		bool add_occupancy = true,
		Ref<RandomNumberGenerator> variation_rng = Ref<RandomNumberGenerator>()
	);
	void add_tile_ellipse(
		int layer_i,
		int tile_i,
		const Vector2i &seg_gpos,
		const Vector2i &g_size,
		bool add_occupancy = true,
		Ref<RandomNumberGenerator> tile_variation_rng = Ref<RandomNumberGenerator>()
	);
	void add_tiles_ellipse(
		const PackedInt32Array &layer_indexes,
		const PackedInt32Array &tile_indexes,
		const Vector2i &seg_gpos,
		const Vector2i &g_size,
		bool add_occupancy = true,
		Ref<RandomNumberGenerator> tile_variation_rng = Ref<RandomNumberGenerator>()
	);
	int add_rand_agnostic_ellipse(
		Ref<RandomNumberGenerator> rng,
		int layer_i,
		int tile_i,
		Vector2i g_size,
		int count = 1,
		Ref<SubgridProbe> advance_bucket = Ref<SubgridProbe>(),
		bool add_occupancy = true,
		bool use_tile_variations = false
	);
	void add_rand(
		Ref<RandomNumberGenerator> rng,
		int layer_i,
		int tile_i,
		int count = 1,
		Ref<SubgridProbe> advance_bucket = Ref<SubgridProbe>(),
		bool add_occupancy = true,
		bool use_tile_variations = false
	);
	void add_row(
		const PackedInt32Array &layer_indexes,
		const PackedInt32Array &tile_indexes,
		int seg_gpos_y,
		bool add_occupancy = true,
		Ref<RandomNumberGenerator> tile_variation_rng = Ref<RandomNumberGenerator>()
	);
	void fill(
		int tile_i = 255,
		bool add_occupancy = false,
		int layer_i = -1,
		Ref<RandomNumberGenerator> tile_variation_rng = Ref<RandomNumberGenerator>()
	);
	void fill_unoccupied(
		int tile_i = 255,
		bool add_occupancy = false,
		int layer_i = -1,
		Ref<RandomNumberGenerator> tile_variation_rng = Ref<RandomNumberGenerator>()
	);

	void clear_occupancy() const;

	int randi_range_exp(Ref<RandomNumberGenerator> rng, int max, int min = 0);

	void rand_fill_rect(
		Ref<RandomNumberGenerator> rng,
		Fill fill_type,
		LocalVector<Ref<Tile>> tiles,
		Rect2i rect,
		bool skip_dirt = false
	);

	int get_gpos_tile(int layer_i, const Vector2i &seg_gpos) const;
};

VARIANT_ENUM_CAST(PCG::Fill)
