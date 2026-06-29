#include "bit_grid_2d.h"

void BitGrid2D::_bind_methods() {
	ClassDB::bind_static_method(
			"BitGrid2D", D_METHOD("create", "grid_size"), &BitGrid2D::create
	);
	ClassDB::bind_method(D_METHOD("clear"), &BitGrid2D::clear);

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
			D_METHOD("get_grid_size"), &BitGrid2D::get_grid_size
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::VECTOR2I, "grid_size"),
		"", "get_grid_size"
	);
	
	ClassDB::bind_method(
			D_METHOD("set_bitmap", "bitmap"), &BitGrid2D::set_bitmap
	);
	ClassDB::bind_method(
			D_METHOD("get_bitmap"), &BitGrid2D::get_bitmap
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::PACKED_BYTE_ARRAY, "bitmap"),
		"set_bitmap", "get_bitmap"
	);
}

Ref<BitGrid2D> BitGrid2D::create(const Vector2i _grid_size) {
	Ref<BitGrid2D> bit_grid;
	bit_grid.instantiate();
	bit_grid->bitmap.resize((_grid_size.x * _grid_size.y + 7) / 8);
	bit_grid->bitmap.fill(0);
	bit_grid->grid_size = _grid_size;
	bit_grid->cell_count = _grid_size.x * _grid_size.y;
	return bit_grid;
}

int BitGrid2D::gpos_to_cell_i(const Vector2i gpos) const {
	return gpos.y * grid_size.x + gpos.x;
}

bool BitGrid2D::is_gpos_set(const Vector2i gpos) const {
	return is_cell_i_set(gpos_to_cell_i(gpos));
}

void BitGrid2D::set_gpos(const Vector2i gpos) {
	set_cell_i(gpos_to_cell_i(gpos));
}

void BitGrid2D::unset_gpos(const Vector2i gpos) {
	unset_cell_i(gpos_to_cell_i(gpos));
}

bool BitGrid2D::is_cell_i_set(const int cell_i) const {
	ERR_FAIL_COND_V_MSG(
		cell_i >= bitmap.size() * 8 || cell_i < 0, true,
		"cell_i is out of grid bounds"
	);
	return (bitmap[cell_i / 8] >> (cell_i % 8)) & 1;
}

void BitGrid2D::set_cell_i(const int cell_i) {
	ERR_FAIL_COND_MSG(
			cell_i < 0 || cell_i >= bitmap.size() * 8,
		"cell_i out of grid bounds"
	);
	uint8_t *data = bitmap.ptrw();
	data[cell_i / 8] |= 1 << (cell_i % 8);
}

void BitGrid2D::unset_cell_i(const int cell_i) {
	ERR_FAIL_COND_MSG(
		cell_i < 0 || cell_i >= bitmap.size() * 8,
		"cell_i out of grid bounds"
	);
	uint8_t *data = bitmap.ptrw();
	data[cell_i / 8] &= ~(1 << (cell_i % 8));
}

void BitGrid2D::set_area(const Vector2i origin, const Vector2i size) {
	ERR_FAIL_COND_MSG(
		size.x <= 0 || size.y <= 0, "provided size is zero area"
	);
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

bool BitGrid2D::is_area_free(const Vector2i origin, const Vector2i size) const {
	ERR_FAIL_COND_V_MSG(
		size.x <= 0 || size.y <= 0, false, "provided size is zero area"
	);
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

int BitGrid2D::find_cell_state(
	int start_cell, int end_cell, bool get_unset
) const {
	const uint8_t *data = bitmap.ptr();

	const int start_byte{ start_cell / 8 };
	const int start_bit{ start_cell % 8 };
	const int end_byte{ end_cell / 8 };
	const int end_bit{ end_cell % 8 };

	const bool wrap{ end_byte < start_byte };
	int it_end_byte{ MAX(cell_count / 8, end_byte) };

	int i{ start_byte };

	if (start_bit > 0) {
		uint8_t test_byte{ get_unset ? ~data[i] : data[i] };
		uint8_t mask{ 0xFF << start_bit };

		test_byte &= mask;

		if (test_byte != 0) {
			return i * 8 + __builtin_ctz(test_byte);
		}
		++i;
	}

	if (i >= end_byte && !wrap) {
		return -1;
	}

	for (int j{ 0 }; j < static_cast<int>(wrap) + 1; ++j) {

		for (; i + 8 <= it_end_byte; i += 8) {
			uint64_t chunk;
			memcpy(&chunk, data + i, 8);
			if (get_unset) {
				chunk = ~chunk;
			}

			if (chunk != 0) {
				return i * 8 + __builtin_ctzll(chunk);
			}
		}

		for (; i < it_end_byte; i++) {
			uint8_t test_byte{ get_unset ? ~data[i] : data[i] };

			if (test_byte != 0) {
				return i * 8 + __builtin_ctz(test_byte);
			}
		}

		if (wrap) {
			i = 0;
			it_end_byte = end_byte;
		}
	}

	if (end_bit > 0) {
		uint8_t test_byte{ get_unset ? ~data[i] : data[i] };
		uint8_t mask{ 0xFF >> (8 - end_bit) };
		test_byte &= mask;
		if (test_byte != 0) {
			return i * 8 + __builtin_ctz(test_byte);
		}
	}

	return -1;
}

int BitGrid2D::find_area_state(
	Vector2i size, int start_cell, bool wrap, bool get_unset
) {
	ERR_FAIL_COND_V_MSG(
		size.x <= 0 || size.y <= 0, -1,
		"area of size provided is smaller or equal to zero"
	);
	ERR_FAIL_COND_V_MSG(
		size.x > grid_size.x || size.y > grid_size.y, -1, "size larger than grid"
	);
	ERR_FAIL_COND_V_MSG(
		start_cell < 0 || start_cell >= bitmap.size() * 8, -1,
		"cell_offset out of bounds"
	);

}
