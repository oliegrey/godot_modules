#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"

class RandomNumberGenerator;

class BitGrid2D : public RefCounted {
	GDCLASS(BitGrid2D, RefCounted);

public:
	enum Direction {NONE = -1, UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3, MAX = 4};

private:
	Vector2i grid_size;
	int cell_count;
	PackedByteArray bitmap;

private:
	int gpos_to_cell_i(const Vector2i gpos) const;

	int find_is_area_state(int start_cell_i, Vector2i size, bool get_unset) const;

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

	bool is_area_free(const Vector2i origin, const Vector2i) const;
	void set_area(const Vector2i gpos, const Vector2i size);
	PackedVector2Array get_max_area_in_state(
		Vector2i origin, Vector2i search_size, const Direction dir
	);

	int find_cell_in_state(
		int end_cell_inc, int start_cell = 0, bool get_unset = true
	) const;

	int find_area_in_state(
		Vector2i size, int start_cell, int end_cell, bool get_unset = true
	);

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
};
