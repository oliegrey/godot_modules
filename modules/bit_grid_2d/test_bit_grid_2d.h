#pragma once

#include "tests/test_macros.h"
#include "bit_grid_2d.h"

namespace TestBitGrid2D {

Ref<BitGrid2D> bitgrid{ BitGrid2D::create(grid_size) };
Vector2i origin{ 0, 0 };
Vector2i grid_size{ 40, 24 };
Vector2i search_size{ 0, 0 };

// find wanted area or largest areas within sub area

// build a directional histogram from the bitmap
	// correctness of histogram given 
		// directional behaviour
		// early exit behaviour
		// whether it starts at the offset


TEST_CASE("[BitGrid2D] compute_histogram rejects dimensionless search sizes") {
	CHECK(bitgrid->)
}
}
