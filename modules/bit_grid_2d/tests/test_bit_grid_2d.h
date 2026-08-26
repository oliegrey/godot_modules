#pragma once

#include "tests/test_macros.h"
#include "modules/bit_grid_2d/bit_grid_2d.h"
#include "core/math/random_number_generator.h"

namespace TestBitGrid2D {

Ref<RandomNumberGenerator> rng;
Vector2i grid_size{ 40, 24 };
Ref<BitGrid2D> bitgrid{ BitGrid2D::create(grid_size) };
Vector2i origin{ 0, 0 };
Vector2i search_size{ 0, 0 };
LocalVector<Vector2i> test_areas{ { 7, 9 }, grid_size };

void setup_rng(int rng_seed) {
	if (rng == Ref<RandomNumberGenerator>()) {
		rng.instantiate();
	}
	rng->set_seed(rng_seed);
}
///////////////////////////////////
/// find_anchored_area_in_area ////
///////////////////////////////////
TEST_CASE("[BitGrid2D] find_anchored_area_in_area rejects wanted area with negative size") {
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::UP, Vector2i(-1, 0)
				)
				== BitGrid2D::NOT_SET
			);
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::UP, Vector2i(0, -1)
				)
				== BitGrid2D::NOT_SET
			);
		}
	}
}
TEST_CASE("[BitGrid2D] find_anchored_area_in_area returns top left when searching an empty grid using UP anchor") {
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::UP, Vector2i(1, 1)
				)
				== Vector2i(0, 0)
			);
		}
	}
}
TEST_CASE("[BitGrid2D] find_anchored_area_in_area returns top left when searching an empty grid using LEFT anchor") {
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::LEFT, Vector2i(1, 1)
				)
				== Vector2i(0, 0)
			);
		}
	}
}
TEST_CASE("[BitGrid2D] find_anchored_area_in_area returns bottom left when searching an empty grid using DOWN anchor") {
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::DOWN, Vector2i(1, 1)
				)
				== Vector2i(0, area.y - 1)
			);
		}
	}
}
TEST_CASE("[BitGrid2D] find_anchored_area_in_area returns top right when searching an empty grid using RIGHT anchor") {
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::RIGHT, Vector2i(1, 1)
				)
				== Vector2i(area.x - 1, 0)
			);
		}
	}
}
TEST_CASE("[BitGrid2D] find_anchored_area_in_area returns top right when searching an empty grid using RIGHT anchor") {
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::RIGHT, Vector2i(1, 1)
				)
				== Vector2i(area.x - 1, 0)
			);
		}
	}
}
TEST_CASE("[BitGrid2D] find_anchored_area_in_area returns +1 offset corner in the correct direction when searching a grid with corners set using UP anchor") {
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			bitgrid->set_gpos(Vector2i{ 0, 0 });
			bitgrid->set_gpos(Vector2i{ area.x - 1, 0 });
			bitgrid->set_gpos(Vector2i{ 0, area.y - 1 });
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::UP, Vector2i(1, 1)
				)
				== Vector2i(1, 0)
			);
			bitgrid->clear();
		}
	}
}
TEST_CASE("[BitGrid2D] find_anchored_area_in_area returns +1 offset corner in the correct direction when searching a grid with corners set using LEFT anchor") {
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			bitgrid->set_gpos(Vector2i{ 0, 0 });
			bitgrid->set_gpos(Vector2i{ area.x - 1, 0 });
			bitgrid->set_gpos(Vector2i{ 0, area.y - 1 });
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::LEFT, Vector2i(1, 1)
				)
				== Vector2i(0, 1)
			);
			bitgrid->clear();
		}
	}
}
TEST_CASE("[BitGrid2D] find_anchored_area_in_area returns +1 offset corner in the correct direction when searching a grid with corners set using DOWN anchor") {
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			bitgrid->set_gpos(Vector2i{ 0, 0 });
			bitgrid->set_gpos(Vector2i{ area.x - 1, 0 });
			bitgrid->set_gpos(Vector2i{ 0, area.y - 1 });
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::DOWN, Vector2i(1, 1)
				)
				== Vector2i(1, area.y - 1)
			);
			bitgrid->clear();
		}
	}
}
TEST_CASE("[BitGrid2D] find_anchored_area_in_area returns +1 offset corner in the correct direction when searching a grid with corners set using RIGHT anchor") {
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			bitgrid->set_gpos(Vector2i{ 0, 0 });
			bitgrid->set_gpos(Vector2i{ area.x - 1, 0 });
			bitgrid->set_gpos(Vector2i{ 0, area.y - 1 });
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::RIGHT, Vector2i(1, 1)
				)
				== Vector2i(area.x - 1, 1)
			);
			bitgrid->clear();
		}
	}
}
TEST_CASE("[BitGrid2D] find_anchored_area_in_area returns NOT_SET when grid is full") {
	bitgrid->fill();
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(0, 0), area, BitGrid2D::Direction::UP, Vector2i(1, 1)
				)
				== BitGrid2D::NOT_SET
			);
		}
	}
	bitgrid->clear();
}
TEST_CASE("[BitGrid2D] find_anchored_area_in_area returns NOT_SET when area is out of bounds") {
	bitgrid->fill();
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			CHECK(
				bitgrid->find_anchored_area_in_area(
					area, area, BitGrid2D::Direction::UP, Vector2i(1, 1)
				)
				== BitGrid2D::NOT_SET
			);
		}
	}
	bitgrid->clear();
}

TEST_CASE("[BitGrid2D] find_anchored_area_in_area does not return wanted size when partially out of bounds") {
	bitgrid->fill();
	for (Vector2i area : test_areas) {
		SUBCASE(vformat("testing area %s", area).utf8().get_data()) {
			CHECK(
				bitgrid->find_anchored_area_in_area(
					Vector2i(grid_size.x - 1, 0),
					Vector2i(8, 8),
					BitGrid2D::Direction::LEFT,
					Vector2i(2, 2)
				)
				== BitGrid2D::NOT_SET
			);
		}
	}
	bitgrid->clear();
}

////////////////////////////////////////////
/// find_largest_anchored_areas_in_area ////
////////////////////////////////////////////
TEST_CASE("[BitGrid2D] find_largest_anchored_areas_in_area wanted_size found") {
	LocalVector<Rect2i> expected_areas{
		Rect2i{ Vector2i{ 4, 0 }, Vector2i{ 2, 2 } },
		Rect2i{ Vector2i{ 2, 0 }, Vector2i{ 1, 8 } },
		Rect2i{ Vector2i{ 6, 0 }, Vector2i{ 2, 8 } }
	};
	bitgrid->set_rect(Vector2i{0, 1}, Vector2i{2, 2});
	bitgrid->set_gpos(Vector2i{ 3, 0 });
	LocalVector<Rect2i> result_areas{
		bitgrid->find_largest_anchored_areas_in_area(
			Vector2i(0, 0),
			Vector2i(8, 8),
			BitGrid2D::Direction::UP,
			Ref<RandomNumberGenerator>(),
			Vector2i(2, 2)
		)
	};
	CHECK(result_areas.size() == expected_areas.size());

	for (uint32_t i{ 0 }; i < MIN(result_areas.size(), expected_areas.size()); ++i) {
		CHECK(expected_areas[i] == result_areas[i]);
	}
	bitgrid->clear();
}
TEST_CASE("[BitGrid2D] find_largest_anchored_areas_in_area no wanted_size found") {
	LocalVector<Rect2i> expected_areas{
		Rect2i{ Vector2i{ 2, 0 }, Vector2i{ 1, 8 } },
		Rect2i{ Vector2i{ 4, 0 }, Vector2i{ 4, 8 } }
	};
	bitgrid->set_rect(Vector2i{0, 1}, Vector2i{2, 2});
	bitgrid->set_gpos(Vector2i{ 3, 0 });
	LocalVector<Rect2i> result_areas{
		bitgrid->find_largest_anchored_areas_in_area(
			Vector2i(0, 0),
			Vector2i(8, 8),
			BitGrid2D::Direction::UP,
			Ref<RandomNumberGenerator>()
		)
	};
	CHECK(result_areas.size() == expected_areas.size());

	for (uint32_t i{ 0 }; i < MIN(result_areas.size(), expected_areas.size()); ++i) {
		CHECK(expected_areas[i] == result_areas[i]);
	}
	bitgrid->clear();
}
TEST_CASE("[BitGrid2D] find_largest_anchored_areas_in_area is full") {
	bitgrid->set_rect(Vector2i{0, 0}, Vector2i{8, 8});
	LocalVector<Rect2i> result_areas{
		bitgrid->find_largest_anchored_areas_in_area(
			Vector2i(0, 0),
			Vector2i(8, 8),
			BitGrid2D::Direction::UP,
			Ref<RandomNumberGenerator>(),
			Vector2i(2, 2)
		)
	};
	CHECK(result_areas.size() == 0);
	bitgrid->clear();
}
TEST_CASE("[BitGrid2D] find_largest_anchored_areas_in_area returns nothing when out of grid bounds") {
	LocalVector<Rect2i> result_areas{
		bitgrid->find_largest_anchored_areas_in_area(
			grid_size,
			Vector2i(8, 8),
			BitGrid2D::Direction::UP,
			Ref<RandomNumberGenerator>(),
			Vector2i(2, 2)
		)
	};
	CHECK(result_areas.size() == 0);
}
TEST_CASE("[BitGrid2D] find_largest_anchored_areas_in_area does not return wanted size when search area is reduced to smaller than it by grid size") {
	bitgrid->clear();
	LocalVector<Rect2i> result_areas{
		bitgrid->find_largest_anchored_areas_in_area(
			Vector2i(grid_size.x - 1, 0),
			Vector2i(8, 8),
			BitGrid2D::Direction::LEFT,
			Ref<RandomNumberGenerator>(),
			Vector2i(2, 2)
		)
	};
	CHECK(result_areas.size() == 1);
	CHECK(result_areas[0] == Rect2i{ Vector2i{grid_size.x - 1, 0}, Vector2i{1, 8} });
}

}
