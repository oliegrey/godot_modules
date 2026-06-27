#pragma once

#include "core/object/ref_counted.h"

class BitGrid2D : public RefCounted {
	GDCLASS(BitGrid2D, RefCounted);

private:
	Vector2i grid_size;
	PackedByteArray bitmap;

private:
	int gpos_to_cell_i(const Vector2i gpos);

protected:
	static void _bind_methods();

public:
	static Ref<BitGrid2D> create(const Vector2i _grid_size);

	Vector2i get_grid_size() const { return grid_size; }
	void set_bitmap(const PackedByteArray &_bitmap) { bitmap = _bitmap; }
	PackedByteArray get_bitmap() const { return bitmap; }
	void fill() { bitmap.fill(255); }
	void clear() { bitmap.fill(0); }
	
	bool is_gpos_set(const Vector2i gpos);
	void set_gpos(const Vector2i gpos);
	void unset_gpos(const Vector2i gpos);

	bool is_cell_i_set(const int cell_i);
	void set_cell_i(const int cell_i);
	void unset_cell_i(const int cell_i);

	bool is_area_free(const Vector2i origin, const Vector2i);
	void set_area(const Vector2i gpos, const Vector2i size);
};
