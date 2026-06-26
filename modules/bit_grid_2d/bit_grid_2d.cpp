#include "bit_grid_2d.h"

void BitGrid2D::_bind_methods() {
	ClassDB::bind_method(
		D_METHOD("is_gpos_set", "gpos"), &BitGrid2D::is_gpos_set
	);
	ClassDB::bind_method(
		D_METHOD("set_gpos", "gpos"), &BitGrid2D::set_gpos
	);
	ClassDB::bind_method(
		D_METHOD("unset_gpos", "gpos"), &BitGrid2D::unset_gpos
	);
	ClassDB::bind_method(
		D_METHOD("is_cell_i_set", "cell_i"), &BitGrid2D::is_cell_i_set
	);
	ClassDB::bind_method(
		D_METHOD("set_cell_i", "cell_i"), &BitGrid2D::set_cell_i
	);
	ClassDB::bind_method(
		D_METHOD("unset_cell_i", "cell_i"), &BitGrid2D::unset_cell_i
	);
	ClassDB::bind_method(
		D_METHOD("set_area", "origin", "size"), &BitGrid2D::set_area
	);
	ClassDB::bind_method(
		D_METHOD("is_area_free", "origin", "size"), &BitGrid2D::is_area_free
	);

	ClassDB::bind_method(
			D_METHOD("set_grid_size", "grid_size"), &BitGrid2D::set_grid_size);
	ClassDB::bind_method(
			D_METHOD("get_grid_size"), &BitGrid2D::get_grid_size);
	ADD_PROPERTY(
		PropertyInfo(Variant::VECTOR2I, "grid_size"),
		"set_grid_size", "get_grid_size"
	);
	
	ClassDB::bind_method(
			D_METHOD("set_bitmap", "bitmap"), &BitGrid2D::set_bitmap);
	ClassDB::bind_method(
			D_METHOD("get_bitmap"), &BitGrid2D::get_bitmap);
	ADD_PROPERTY(
		PropertyInfo(Variant::PACKED_BYTE_ARRAY, "bitmap"),
		"set_bitmap", "get_bitmap"
	);
}

int BitGrid2D::gpos_to_cell_i(const Vector2i gpos) {
	return gpos.y * grid_size.x + gpos.x;
}

bool BitGrid2D::is_gpos_set(const Vector2i gpos) {
	return is_cell_i_set(gpos_to_cell_i(gpos));
}

void BitGrid2D::set_gpos(const Vector2i gpos) {
	set_cell_i(gpos_to_cell_i(gpos));
}

void BitGrid2D::unset_gpos(const Vector2i gpos) {
	unset_cell_i(gpos_to_cell_i(gpos));
}

bool BitGrid2D::is_cell_i_set(const int cell_i) {
	DEV_ASSERT(!bitmap.is_empty());
	ERR_FAIL_COND_V_MSG(
		cell_i >= bitmap.size() * 8 || cell_i < 0, true,
		"cell_i is out of grid bounds"
	);

	return (bitmap[cell_i / 8] >> (cell_i % 8)) & 1;
}

void BitGrid2D::set_cell_i(const int cell_i) {
	DEV_ASSERT(!bitmap.is_empty());
	ERR_FAIL_COND_MSG(
			cell_i < 0 || cell_i >= bitmap.size() * 8,
		"cell_i out of grid bounds"
	);
	uint8_t *data = bitmap.ptrw();
	data[cell_i / 8] |= 1 << (cell_i % 8);
}

void BitGrid2D::unset_cell_i(const int cell_i) {
	DEV_ASSERT(!bitmap.is_empty());
	ERR_FAIL_COND_MSG(
		cell_i < 0 || cell_i >= bitmap.size() * 8,
		"cell_i out of grid bounds"
	);
	uint8_t *data = bitmap.ptrw();
	data[cell_i / 8] &= ~(1 << (cell_i % 8));
}

void BitGrid2D::set_area(const Vector2i origin, const Vector2i size) {
	DEV_ASSERT(!bitmap.is_empty());
	DEV_ASSERT(size > Vector2i(0, 0))
	ERR_FAIL_COND_MSG(
		origin < Vector2i(0, 0) || origin + size > grid_size,
		"provided origin + size out of grid bounds"
	);

	uint8_t *data = bitmap.ptrw();
	for (int y{ origin.y }; y < origin.y + size.y; y++) {
		for (int x{ origin.x }; x < origin.x + size.x; x++) {
			const int cell_i{ gpos_to_cell_i(Vector2i(x, y)) };
			data[cell_i / 8] |= 1 << (cell_i % 8);
		}
	}
}

bool BitGrid2D::is_area_free(const Vector2i origin, const Vector2i size) {
	DEV_ASSERT(!bitmap.is_empty());
	DEV_ASSERT(size > Vector2i(0, 0))
	ERR_FAIL_COND_V_MSG(
		origin < Vector2i(0, 0) || origin + size > grid_size, false,
		"provided origin + size out of grid bounds"
	);

	for (int y{ origin.y }; y < origin.y + size.y; y++) {
		for (int x{ origin.x }; x < origin.x + size.x; x++) {
			const int cell_i{ gpos_to_cell_i(Vector2i(x, y)) };

			if ((bitmap[cell_i / 8] >> (cell_i % 8)) & 1) {
				return false;
			}
		}
	}
	return true;
}
