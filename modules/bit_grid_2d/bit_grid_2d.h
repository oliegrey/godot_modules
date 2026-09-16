#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"
#include "modules/direction/direction.h"
#include "modules/axis/axis.h"
#include "core/math/random_number_generator.h"

class BitGrid2D : public RefCounted {
	GDCLASS(BitGrid2D, RefCounted);

public:
	struct HistogramResult {
		int wanted_cell_i;
		PackedInt32Array histogram;

		HistogramResult() : wanted_cell_i{ -1 } { }
		HistogramResult(int _wanted_cell_i, const PackedInt32Array &_histogram) :
			wanted_cell_i{ _wanted_cell_i }, histogram{ _histogram }
		{ }
	};

private:
	Vector2i grid_size;
	int cell_count;
	PackedByteArray bitmap;

public:
	inline static const Vector2i NOT_SET{ -9999, -9999 };

private:
	int gpos_to_cell_i(const Vector2i gpos) const;
	int is_area_cell_state(int start_cell_i, Vector2i size, bool get_unset) const;
	bool clamp_search_area(Vector2i &origin, Vector2i &search_size) const;
	HistogramResult compute_histogram(
		Vector2i origin,
		Vector2i size,
		Direction::E anchor_dir,
		int start_bar = 0,
		Vector2i wanted_size = Vector2i(0, 0),
		bool exit_on_wanted_found = false
	) const;

protected:
	static void _bind_methods();

public:
	static Ref<BitGrid2D> create(const Vector2i _grid_size);

	Vector2i get_grid_size() const { return grid_size; }
	void set_bitmap(const PackedByteArray &_bitmap) { bitmap = _bitmap; }
	PackedByteArray get_bitmap() const { return bitmap; }
	void fill() { bitmap.fill(255); }
	void clear() { bitmap.fill(0); }

	bool is_empty() const;
	
	bool is_gpos_set(const Vector2i gpos) const;
	void set_gpos(const Vector2i gpos);
	void unset_gpos(const Vector2i gpos);

	bool is_cell_i_set(const int cell_i) const;
	void set_cell_i(const int cell_i);
	void unset_cell_i(const int cell_i);

	bool is_rect_state(const Rect2i &rect, const bool is_set = false) const;

	void set_rect(const Rect2i &rect);

	Vector2i find_rand_anchored_unset_area_in_bounds(
		Ref<RandomNumberGenerator> rng,
		Vector2i bounds_origin,
		Vector2i bounds_size,
		const Direction::E anchor_dir,
		Vector2i wanted_size
	) const;

	int find_cell_in_state(int end_cell_inc, int start_cell = 0, bool get_unset = true) const;

	int find_area_in_grid(Vector2i size, int start_cell, int end_cell, bool get_unset = true) const;

	Vector2i find_rand_gpos_in_state(
		Ref<RandomNumberGenerator> rng,
		Vector2i required_size = Vector2i(1, 1),
		bool get_unset = true
	);

	Vector2i find_rand_gpos_ranged_in_state(
		Ref<RandomNumberGenerator> rng,
		Vector2i search_y_range_ex,
		Vector2i search_x_range_ex = Vector2i(),
		Vector2i size = Vector2i(1, 1),
		bool get_unset = true
	);

	int find_rand_area_in_area(
		Ref<RandomNumberGenerator> rng,
		Vector2i origin,
		Vector2i wanted_size,
		Vector2i search_size
	) const;

	int find_area_in_area(
		Vector2i origin,
		Vector2i wanted_size,
		Vector2i search_size,
		Vector2i search_start_gpos = Vector2i(-1, -1)
	) const;

	Vector2i find_anchored_area_in_area(
		Vector2i origin,
		Vector2i search_size,
		Direction::E anchor_dir,
		Vector2i wanted_size,
		Ref<RandomNumberGenerator> rng = Ref<RandomNumberGenerator>()
	) const;

	LocalVector<Rect2i> find_largest_anchored_areas_in_area(
		Vector2i origin,
		Vector2i search_size,
		Direction::E anchor_dir,
		Ref<RandomNumberGenerator> rng = Ref<RandomNumberGenerator>(),
		Vector2i wanted_size = Vector2i(0, 0)
	) const;
};
