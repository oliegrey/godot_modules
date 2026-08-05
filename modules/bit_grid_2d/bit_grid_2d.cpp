#include "bit_grid_2d.h"
#include "core/math/random_number_generator.h"

#include <functional>

#if defined(_MSC_VER)
#include <intrin.h>
static inline int ctz32(uint32_t x) {
	unsigned long index;
	_BitScanForward(&index, x);
	return static_cast<int>(index);
}
static inline int ctz64(uint64_t x) {
	unsigned long index;
	_BitScanForward64(&index, x);
	return static_cast<int>(index);
}
#else
static inline int ctz32(uint32_t x) {
	return __builtin_ctz(x);
}
static inline int ctz64(uint64_t x) {
	return __builtin_ctzll(x);
}
#endif

void BitGrid2D::_bind_methods() {
	ClassDB::bind_static_method(
		"BitGrid2D", D_METHOD("create", "grid_size"), &BitGrid2D::create
	);
	ClassDB::bind_method(
		D_METHOD("is_empty"), &BitGrid2D::is_empty
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
		D_METHOD(
			"find_cell_in_state",
			"end_cell_inc",
			"start_cell",
			"get_unset"
		),
		&BitGrid2D::find_cell_in_state,
		DEFVAL(0),
		DEFVAL(true)
	);

	ClassDB::bind_method(
		D_METHOD(
			"find_area_in_grid",
			"size",
			"start_cell",
			"end_cell_inc",
			"get_unset"
		),
		&BitGrid2D::find_area_in_grid,
		DEFVAL(true)
	);
	ClassDB::bind_method(
		D_METHOD(
			"find_anchored_unset_areas_in_bounds",
			"origin",
			"search_size",
			"anchor_dir",
			"rng",
			"wanted_size"
		),
		&BitGrid2D::find_anchored_unset_areas_in_bounds, DEFVAL(Vector2i())
	);

	ClassDB::bind_method(
		D_METHOD(
			"find_rand_gpos_in_state",
			"rng",
			"required_size",
			"get_unset"
		),
		&BitGrid2D::find_rand_gpos_in_state,
		DEFVAL(Vector2i(1, 1)),
		DEFVAL(true)
	);

	ClassDB::bind_method(
		D_METHOD(
			"find_rand_gpos_ranged_in_state",
			"rng",
			"search_y_range",
			"search_x_range",
			"size",
			"get_unset"
		),
		&BitGrid2D::find_rand_gpos_ranged_in_state,
		DEFVAL(Vector2i()),
		DEFVAL(Vector2i(1, 1)),
		DEFVAL(true)
	);

	ClassDB::bind_method(
		D_METHOD(
			"find_rand_area_in_area",
			"rng",
			"origin",
			"wanted_size",
			"search_size"
		),
		&BitGrid2D::find_rand_area_in_area
	);

	ClassDB::bind_method(
		D_METHOD(
			"find_area_in_area",
			"origin",
			"wanted_size",
			"search_size",
			"search_start_gpos"
		),
		&BitGrid2D::find_area_in_area,
		DEFVAL(Vector2i(-1, -1))
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

	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(UP);
	BIND_ENUM_CONSTANT(DOWN);
	BIND_ENUM_CONSTANT(LEFT);
	BIND_ENUM_CONSTANT(RIGHT);
	BIND_ENUM_CONSTANT(DIRECTION_MAX);

	BIND_ENUM_CONSTANT(X);
	BIND_ENUM_CONSTANT(Y);
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

bool BitGrid2D::is_empty() const {
	const uint8_t *data = bitmap.ptr();
	int64_t size = bitmap.size();

	int64_t chunk_i{ 0 };
	int64_t chunk_count = size / 8;
	const uint64_t *chunk_64_ptr = reinterpret_cast<const uint64_t *>(data);
	for (; chunk_i < chunk_count; ++chunk_i) {
		if (*(chunk_64_ptr + chunk_i) != 0) {
			return false;
		}
	}

	for (int64_t tail_byte_i{ chunk_i * 8 }; tail_byte_i < size; ++tail_byte_i) {
		if (data[tail_byte_i] != 0) {
			return false;
		}
	}

	return true;
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
		cell_i >= cell_count || cell_i < 0, true,
		"cell_i is out of grid bounds"
	);
	return (bitmap[cell_i / 8] >> (cell_i % 8)) & 1;
}

void BitGrid2D::set_cell_i(const int cell_i) {
	ERR_FAIL_COND_MSG(
		cell_i < 0 || cell_i >= cell_count,
		"cell_i out of grid bounds"
	);
	uint8_t *data = bitmap.ptrw();
	data[cell_i / 8] |= 1 << (cell_i % 8);
}

void BitGrid2D::unset_cell_i(const int cell_i) {
	ERR_FAIL_COND_MSG(
		cell_i < 0 || cell_i >= cell_count,
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

// CAREFUL: HALF OF THIS FUNCTION IS AI DOGSHIT
// returns origin, size pairs
// // // // //
// origin is top left cell of search size.
// wanted_size == (0,0): build full histogram, return largest unset rect per run.
// wanted_size given: return as soon as a fit is found, without finishing the sweep.
// rng valid: start the bar sweep at a random offset so repeated "wanted"
// queries don't always return the same leftmost/topmost fit.
PackedVector2Array BitGrid2D::find_anchored_unset_areas_in_bounds(
	Vector2i origin,
	Vector2i search_size,
	const Direction anchor_dir,
	Ref<RandomNumberGenerator> rng,
	Vector2i wanted_size
) const {
	if (search_size.x <= 0 || search_size.y <= 0) {
		return PackedVector2Array();
	}

	if (origin.x < 0) {
		search_size.x += origin.x;
		origin.x = 0;
	}
	if (origin.y < 0) {
		search_size.y += origin.y;
		origin.y = 0;
	}
	if (origin.x + search_size.x >= grid_size.x) {
		int delta{ origin.x + search_size.x - grid_size.x };
		search_size.x -= delta;
	}
	if (origin.y + search_size.y >= grid_size.y) {
		int delta{ origin.y + search_size.y - grid_size.y };
		search_size.y -= delta;
	}

	if (search_size.x < wanted_size.x || search_size.y < wanted_size.y) {
		wanted_size = Vector2i(0, 0);
	}

	const bool has_wanted{ wanted_size.x > 0 && wanted_size.y > 0 };
	ERR_FAIL_COND_V_MSG(
		has_wanted && (wanted_size.x > search_size.x || wanted_size.y > search_size.y),
		PackedVector2Array(), "wanted size larger than search size"
	);

	Vector2i hist_origin{ origin };
	int hist_polarity{ 1 };
	Axis hist_axis{};
	switch (anchor_dir) {
		case Direction::DOWN: {
			hist_polarity = -1;
			hist_origin.y += search_size.y - 1;
			[[fallthrough]];
		}
		case Direction::UP: {
			hist_axis = Axis::Y;
			break;
		}
		case Direction::RIGHT: {
			hist_polarity = -1;
			hist_origin.x += search_size.x - 1;
			[[fallthrough]];
		}
		case Direction::LEFT: {
			hist_axis = Axis::X;
			break;
		}
		default:
			return PackedVector2Array();
	}

	const int hist_max_dist{ search_size[hist_axis] };
	const int max_bar_count{ search_size[1 - hist_axis] };
	const int wanted_height{ has_wanted ? wanted_size[hist_axis] : 0 };
	const int wanted_width{ has_wanted ? wanted_size[1 - hist_axis] : 0 };

	const int base_cell_i{ gpos_to_cell_i(hist_origin) };
	Vector2i axis_unit;
	axis_unit[hist_axis] = 1;
	Vector2i bar_unit;
	bar_unit[1 - hist_axis] = 1;
	const int step_axis{ (gpos_to_cell_i(hist_origin + axis_unit) - base_cell_i) * hist_polarity };
	const int step_bar{ gpos_to_cell_i(hist_origin + bar_unit) - base_cell_i };

	// offset == 0 (no rng, or degenerate range) keeps this identical to the unrotated path.
	int offset{ 0 };
	if (rng.is_valid() && max_bar_count > 1) {
		offset = rng->randi_range(0, max_bar_count - 1);
	}
	const bool has_offset{ offset != 0 };
	// virtual index of the synthetic wall bar separating [offset, max) from [0, offset)
	const int wrap_pos{ max_bar_count - offset };
	const int total_bars{ has_offset ? max_bar_count + 1 : max_bar_count };

	// maps a virtual (wall-shifted) bar index back to its real spatial column
	auto real_left = [&](int vi) -> int {
		if (!has_offset) {
			return vi;
		}
		if (vi < wrap_pos) {
			return offset + vi;
		}
		return vi - wrap_pos - 1;
	};

	// Heights computed on demand, fed straight into the stack sweep in the same loop:
	// lets "wanted" mode short-circuit before the histogram is fully built.
	Vector<int> heights;
	heights.resize(total_bars + 1);
	int *heights_ptr{ heights.ptrw() };
	const uint8_t *bitmap_ptr{ bitmap.ptr() };

	Vector<int> stack_idx;
	stack_idx.resize(total_bars + 2);
	int *stack_ptr{ stack_idx.ptrw() };
	int stack_top{ -1 }; // -1 == empty

	PackedVector2Array result{};

	int best_area{ 0 };
	Vector2i best_origin{};
	Vector2i best_size{};

	int row_cell_i{ base_cell_i + step_bar * offset };

	for (int vi{ 0 }; vi <= total_bars; ++vi) {
		int cur_height{ 0 };
		const bool is_wall{ has_offset && vi == wrap_pos };

		if (vi < total_bars) {
			if (is_wall) {
				// zero-height wall: real bars max-1 and 0 aren't spatially adjacent, so force the run to close here
				heights_ptr[vi] = 0;
				cur_height = 0;
				row_cell_i = base_cell_i;
			} else {
				int cell_i{ row_cell_i };
				int hist_dist{ 0 };
				for (; hist_dist < hist_max_dist; ++hist_dist, cell_i += step_axis) {
					const bool is_cell_set{ ((bitmap_ptr[cell_i >> 3] >> (cell_i & 7)) & 1) == 1 };
					if (is_cell_set) {
						break;
					}
				}
				heights_ptr[vi] = hist_dist;
				cur_height = hist_dist;
				row_cell_i += step_bar;
			}
		}

		while (stack_top >= 0 && heights_ptr[stack_ptr[stack_top]] >= cur_height) {
			const int top{ stack_ptr[stack_top] };
			--stack_top;
			const int height{ heights_ptr[top] };
			if (height <= 0) {
				continue; // gap bar or seam wall, not a real rectangle
			}
			const int left{ stack_top < 0 ? 0 : stack_ptr[stack_top] + 1 };
			const int width{ vi - left };
			const int spatial_left{ real_left(left) };

			if (has_wanted && height >= wanted_height && width >= wanted_width) {
				Vector2i found_origin{};
				Vector2i found_size{};
				found_origin[hist_axis] = hist_origin[hist_axis];
				found_origin[1 - hist_axis] = hist_origin[1 - hist_axis] + spatial_left;
				found_size[hist_axis] = wanted_height;
				found_size[1 - hist_axis] = wanted_width;

				result.clear();
				result.push_back(found_origin);
				result.push_back(found_size);
				return result;
			}

			const int area{ height * width };
			if (area > best_area) {
				best_area = area;
				best_origin[hist_axis] = hist_origin[hist_axis];
				best_origin[1 - hist_axis] = hist_origin[1 - hist_axis] + spatial_left;
				best_size[hist_axis] = height;
				best_size[1 - hist_axis] = width;
			}
		}

		if (cur_height <= 0) {
			if (best_area > 0) {
				result.push_back(best_origin);
				result.push_back(best_size);
				best_area = 0;
			}
		}

		stack_ptr[++stack_top] = vi;
	}

	return result;
}

// wraps for the anchor dir (e.g. Direction::UP would wrap on the x axis)
// only finds wanted_size in contrast to the non rand version
Vector2i BitGrid2D::find_rand_anchored_unset_area_in_bounds(
	Ref<RandomNumberGenerator> rng,
	Vector2i bounds_origin,
	Vector2i bounds_size,
	const Direction anchor_dir,
	Vector2i wanted_size
) const {
	Vector2i search_origin{ bounds_origin };
	Vector2i search_size{ bounds_size };

	if (anchor_dir == Direction::UP) {
		search_size.y = wanted_size.y;
	} else if (anchor_dir == Direction::DOWN) {
		search_size.y = wanted_size.y;
		search_origin.y += bounds_size.y - wanted_size.y;
	} else if (anchor_dir == Direction::LEFT) {
		search_size.x = wanted_size.x;
	} else if (anchor_dir == Direction::RIGHT) {
		search_size.x = wanted_size.x;
		search_origin.x += bounds_size.x - wanted_size.x;
	}

	const int cell{ find_rand_area_in_area(rng, search_origin, wanted_size, search_size) };

	if (cell == -1) { return Vector2i(-9999, -9999); }
	return Vector2i{ cell % grid_size.x, cell / grid_size.x };
}

// wraps when end_cell_inc < start_cell
int BitGrid2D::find_cell_in_state(
	int end_cell_inc, int start_cell, bool get_unset
) const {
	const uint8_t *data = bitmap.ptr();

	const int start_byte{ start_cell / 8 };
	const int start_bit{ start_cell % 8 };
	const int end_byte{ end_cell_inc / 8 };
	const int end_bit{ end_cell_inc % 8 };

	const bool wrap{ end_cell_inc < start_cell };
	int it_end_byte{ wrap ? cell_count / 8 : MIN(cell_count / 8, end_byte) };

	int byte_i{ start_byte };

	const bool in_one_byte{ start_byte == end_byte && !wrap };

	if (start_bit > 0 || in_one_byte) {
		uint8_t test_byte{
			static_cast<uint8_t>(get_unset ? ~data[byte_i] : data[byte_i])
		};
		uint8_t mask{ static_cast<uint8_t>(0xFF << start_bit) };
		if (in_one_byte) {
			mask &= 0xFF >> (7 - end_bit);
		}

		test_byte &= mask;

		if (test_byte != 0) {
			return byte_i * 8 + ctz32(test_byte);
		}
		if (in_one_byte) {
			return -1;
		}

		++byte_i;
	}

	for (int j{ 0 }; j < static_cast<int>(wrap) + 1; ++j) {

		for (; byte_i + 8 <= it_end_byte; byte_i += 8) {
			uint64_t chunk;
			memcpy(&chunk, data + byte_i, 8);
			if (get_unset) {
				chunk = ~chunk;
			}

			if (chunk != 0) {
				return byte_i * 8 + ctz64(chunk);
			}
		}

		for (; byte_i < it_end_byte; byte_i++) {
			uint8_t test_byte{
				static_cast<uint8_t>(get_unset ? ~data[byte_i] : data[byte_i])
			};

			if (test_byte != 0) {
				return byte_i * 8 + ctz32(test_byte);
			}
		}

		if (wrap && j == 0) {
			byte_i = 0;
			it_end_byte = end_byte;
		}
	}

	if (end_bit > 0) {
		uint8_t test_byte{
			static_cast<uint8_t>(get_unset ? ~data[byte_i] : data[byte_i])
		};
		uint8_t mask{ static_cast<uint8_t>(0xFF >> (7 - end_bit)) };
		test_byte &= mask;
		if (test_byte != 0) {
			return byte_i * 8 + ctz32(test_byte);
		}
	}

	return -1;
}

// wraps grid
int BitGrid2D::find_rand_area_in_area(
	Ref<RandomNumberGenerator> rng,
	Vector2i origin,
	Vector2i wanted_size,
	Vector2i search_size
) const {
	Vector2i rand_search_area_gpos {
		rng->randi_range(0, search_size.x - wanted_size.x),
		rng->randi_range(0, search_size.y - wanted_size.y)
	};
	Vector2i rand_search_gpos{ origin + rand_search_area_gpos };
	return find_area_in_area(origin, wanted_size, search_size, rand_search_gpos);
}

// wraps grid
int BitGrid2D::find_area_in_area(
	Vector2i origin,
	Vector2i wanted_size,
	Vector2i search_size,
	Vector2i search_start_gpos
) const {
	ERR_FAIL_COND_V_MSG(
		search_size.x < wanted_size.x || search_size.y < wanted_size.y, -1,
		vformat(
			"search_size(%s) not large enough to accommodate wanted_size(%s)",
			search_size, wanted_size
		)
	);
	
	if (search_start_gpos == Vector2i(-1, -1)) {
		search_start_gpos = origin;
	}
	// start x search - from the random x position to the end of the row
	const int search_start_cell{ search_start_gpos.x + search_start_gpos.y * grid_size.x };
	const int search_start_offset_x{ search_start_gpos.x - origin.x };
	const int row_end_dist{ search_size.x - search_start_offset_x };
	const int first_row_end_cell{ search_start_cell + row_end_dist - wanted_size.x };

	int cell{ find_area_in_grid(wanted_size, search_start_cell, first_row_end_cell) };
	if (cell != -1) { return cell; }

	// start y search - from the random y to the end of the search_size.y
	int upper_search_bounds_y{ origin.y + search_size.y - wanted_size.y };

	for (int y{ search_start_gpos.y + 1 }; y <= upper_search_bounds_y; ++y) {
		int start_cell{ y * grid_size.x + origin.x };
		int end_cell{ start_cell + search_size.x - wanted_size.x };

		cell = find_area_in_grid(wanted_size, start_cell, end_cell);
		if (cell != -1) { return cell; }
	}

	// end y search - from the start of the search_size.y to the random y
	for (int y{ origin.y }; y < search_start_gpos.y; ++y) {
		int start_cell{ y * grid_size.x + origin.x };
		int end_cell{ start_cell + search_size.x - 1 };
		cell = find_area_in_grid(wanted_size, start_cell, end_cell);
		if (cell != -1) { return cell; }
	}

	// remaining x search - from the start of the row to the random x position
	const int search_row_start_cell{ search_start_gpos.y * grid_size.x + origin.x };
	const int search_end_cell{ search_start_cell - wanted_size.x };
	if (search_end_cell >= search_row_start_cell) {
		cell = find_area_in_grid(wanted_size, search_row_start_cell, search_end_cell);
	}

	return cell;
}

// wraps grid
int BitGrid2D::find_area_in_grid(
	Vector2i size, int start_cell, int end_cell_inc, bool get_unset
) const {
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
	int cell_i{ start_cell };
	int cell_dist{ (end_cell_inc - start_cell + cell_count) % cell_count };
	if (cell_dist == 0) { cell_dist = 1; }

	int i{ 0 };
	while (i < cell_dist) {
		cell_i = find_cell_in_state(end_cell_inc, cell_i, get_unset);
		if (cell_i == -1) { return -1; } // no possible areas left

		int cell_advancement{ is_area_state(cell_i, size, get_unset) };
		if (cell_advancement == -1) { return cell_i; } // area found

		i += cell_advancement;
		cell_i = (start_cell + i) % cell_count;
	}
	return -1;
}

int BitGrid2D::is_area_state(int start_cell_i, Vector2i size, bool get_unset) const {
	const int start_x{ start_cell_i % grid_size.x };
	const int start_y{ start_cell_i / grid_size.x };

	// guard against going out of bounds and advance to get back in bounds
	if (start_x + size.x > grid_size.x) {
		return grid_size.x - start_x;
	}
	else if (start_y + size.y > grid_size.y) {
		return cell_count - start_y * grid_size.x;
	}

	int max_x{ 1 };
	for (int y{ 0 }; y < size.y; ++y) {
		for (int x{ static_cast<int>(y == 0) }; x < size.x; ++x) {
			int test_cell_i{ start_cell_i + x + y * grid_size.x };
			int bit_pos{ test_cell_i % 8 };

			uint8_t test_byte{ bitmap[test_cell_i / 8] };
			int test_bit{ test_byte >> bit_pos };

			if (!get_unset) { test_bit = ~test_bit; }

			if (test_bit & 1) { return max_x; }

			max_x = MAX(max_x, x);
		}
	}
	return -1;
}

Vector2i BitGrid2D::find_rand_gpos_in_state(
	Ref<RandomNumberGenerator> rng, Vector2i required_size, bool get_unset
) {
	const int cell_start{ rng->randi_range(0, cell_count - 1) };
	const int cell_end_inc{ cell_start == 0 ? cell_count - 1 : cell_start - 1 };
	int cell_i{ -1 };

	if (required_size == Vector2i(1, 1)) {
		cell_i = find_cell_in_state(cell_end_inc, cell_start, get_unset);
	} else {
		cell_i = find_area_in_grid(required_size, cell_start, cell_end_inc, get_unset);
	}

	if (cell_i == -1) { return Vector2i(-9999, -9999); }
	return Vector2i(cell_i % grid_size.x, cell_i / grid_size.x);
}

Vector2i BitGrid2D::find_rand_gpos_ranged_in_state(
	Ref<RandomNumberGenerator> rng,
	Vector2i search_y_range_ex,
	Vector2i search_x_range_ex,
	Vector2i size,
	bool get_unset
) {
	// keep ranges in bounds, shouldnt matter if y < x due to wrapping
	search_x_range_ex.x = MAX(search_x_range_ex.x, 0);
	search_x_range_ex.y = MIN(search_x_range_ex.y, grid_size.x);
	search_y_range_ex.x = MAX(search_y_range_ex.x, 0);
	search_y_range_ex.y = MIN(search_y_range_ex.y, grid_size.y);

	// get a random starting cell while wrapping correctly
	Vector2i rand_gpos{};
	if (search_x_range_ex.x < search_x_range_ex.y) {
		rand_gpos.x = rng->randi_range(search_x_range_ex.x, search_x_range_ex.y - 1);
	} else {
		int width{ grid_size.x - search_x_range_ex.y + search_x_range_ex.x };
		rand_gpos.x = search_x_range_ex.x + rng->randi_range(0, width);
	}
	if (search_y_range_ex.x < search_y_range_ex.y) {
		rand_gpos.y = rng->randi_range(search_y_range_ex.x, search_y_range_ex.y - 1);
	} else {
		int height{ grid_size.y - search_y_range_ex.y + search_y_range_ex.x };
		rand_gpos.y = search_y_range_ex.x + rng->randi_range(0, height);
	}
	const int rand_cell{ rand_gpos.x + rand_gpos.y * grid_size.x };

	int cell_i{ -1 };

	std::function<int(int, int)> search;
	if (size == Vector2i(1, 1)) {
		search = [&](int start, int end_inc) {
			return find_cell_in_state(end_inc, start, get_unset);
		};
	} else {
		search = [&](int start, int end_inc) {
			return find_area_in_grid(size, start, end_inc, get_unset);
		};
	}

	if (search_x_range_ex == Vector2i()) {
		const int start_cell{ search_y_range_ex.x * grid_size.x };
		const int enc_cell_inc{ search_y_range_ex.y * grid_size.x - 1 };

		if (size == Vector2i(1, 1)) {
			cell_i = search(rand_cell, enc_cell_inc);
			if (cell_i == -1 && start_cell != rand_cell) {
				cell_i = search(start_cell, rand_cell - 1);
			}
			else if (cell_i != -1) {
				return Vector2i(cell_i % grid_size.x, cell_i / grid_size.x);
			}
		} 

	} else {
		bool wrap_x{ search_x_range_ex.x >= search_x_range_ex.y };
		bool wrap_y{ search_y_range_ex.x >= search_y_range_ex.y };
		int start_y_it{ rand_gpos.y };
		int end_y_it{ wrap_y ? grid_size.y : search_y_range_ex.y };
		int start_x_it{ rand_gpos.x };
		int end_x_it{ search_x_range_ex.y };

		if (size == Vector2i(1, 1)) {
			for (int y_it{ 0 }; y_it < 1 + static_cast<int>(wrap_y); ++y_it) {
				for (int y{ start_y_it }; y < end_y_it; ++y) {
					if (wrap_x) {
						end_x_it = grid_size.x;
					}

					for (int x_it{ 0 }; x_it < 1 + static_cast<int>(wrap_x); ++x_it) {
						int cell_start{ start_x_it + y * grid_size.x };
						int cell_end_inc{ end_x_it + y * grid_size.x - 1 };

						cell_i = search(cell_start, cell_end_inc);

						if (cell_i != -1) {
							return Vector2i(cell_i % grid_size.x, cell_i / grid_size.x);
						}

						if (wrap_x && x_it == 0) {
							start_x_it = 0;
							end_x_it = search_x_range_ex.y;
						}
					}
				}

				if (wrap_y && y_it == 0) {
					start_y_it = 0;
					end_y_it = search_y_range_ex.x;
				}
			}
		}
	}

	return Vector2i(-9999, -9999);
}
	

