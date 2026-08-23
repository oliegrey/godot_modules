#pragma once

#include "tests/test_macros.h"
#include "modules/region/region.h"
#include "core/math/random_number_generator.h"
#include "modules/pcg/pcg.h"
#include "modules/bit_grid_2d/bit_grid_2d.h"

namespace TestRegion {

Ref<Region> p_region;
Ref<Region> s_region1;
Ref<Region> s_region2;
Ref<Region> p_region_full_blocked;
Ref<Region> p_region1;
std::array<Ref<Region>, 3> regions;
Region::DirEdge diredge{};
Vector2i grid_size{ 8, 8 };
Ref<RandomNumberGenerator> rng;

void create_regions() {
	p_region = Region::create(
		"p_test", Region::PRIMARY, Vector2i{ 3, 3 }, 1, 0,

		PackedInt32Array{ Region::DOWN, Region::RIGHT },
		PackedInt32Array{ Region::STONE, Region::DIRT },
		PackedInt32Array{ },

		Array{}, Array{}, Array{}, Array{}
	);
	s_region1 = Region::create(
		"s_test1", Region::SECONDARY, Vector2i{ 1, 1 }, 1, 0,

		PackedInt32Array{ Region::LEFT, Region::RIGHT },
		PackedInt32Array{ Region::STONE, Region::MIX },
		PackedInt32Array{ Region::DOWN, Region::UP },

		Array{}, Array{}, Array{}, Array{}
	);
	s_region2 = Region::create(
		"s_test2", Region::SECONDARY, Vector2i{ 2, 2 }, 1, 0,

		PackedInt32Array{ },
		PackedInt32Array{ },
		PackedInt32Array{ Region::LEFT, Region::RIGHT, Region::DOWN, Region::UP },

		Array{}, Array{}, Array{}, Array{}
	);
	p_region_full_blocked = Region::create(
		"s_test_full_blocked", Region::PRIMARY, Vector2i{ 3, 3 }, 1, 0,

		PackedInt32Array{ Region::UP, Region::DOWN, Region::LEFT, Region::RIGHT  },
		PackedInt32Array{ Region::STONE, Region::DIRT, Region::MIX, Region::ANY },
		PackedInt32Array{ },

		Array{}, Array{}, Array{}, Array{}
	);
	p_region1 = Region::create(
		"p_region1", Region::PRIMARY, Vector2i{ 1, 1 }, 1, 0,

		PackedInt32Array{ Region::DOWN },
		PackedInt32Array{ Region::STONE },
		PackedInt32Array{ Region::LEFT, Region::RIGHT, Region::UP },

		Array{}, Array{}, Array{}, Array{}
	);
	regions = std::array<Ref<Region>, 3>{ p_region, s_region1, s_region2 };
}

TEST_CASE("[Region] initialized correctly") {
	rng.instantiate();
	rng->set_seed(0);

	Region::initialize(grid_size, false);
	create_regions();
	Region::finalize();

	SUBCASE("[Region] grid_size set correctly") {
		CHECK(Region::m_seg_g_size == grid_size);
	}

	SUBCASE("[Region] p_region g_size is set correctly") {
		CHECK(p_region->g_size == Vector2i{ 3, 3 });
	}
	SUBCASE("[Region] p_region g_size_inclusive adds left, right and down blocked sides") {
		CHECK(p_region->g_size_inclusive == Vector2i{ 4, 4 });
	}

	SUBCASE("[Region] s_region1 g_size is set correctly") {
		CHECK(s_region1->g_size == Vector2i{ 1, 1 });
	}
	SUBCASE("[Region] s_region1 g_size_inclusive adds left and right blocked sides") {
		CHECK(s_region1->g_size_inclusive == Vector2i{ 3, 1 });
	}

	SUBCASE("[Region] s_region2 g_size is set correctly") {
		CHECK(s_region2->g_size == Vector2i{ 2, 2 });
	}
	SUBCASE("[Region] s_region2 g_size_inclusive does not add blocked sides") {
		CHECK(s_region2->g_size_inclusive == Vector2i{ 2, 2 });
	}
}

TEST_CASE("[Region] edge is added in the correct location, while ignoring blocked sides and pinned at the top") {
	diredge = Region::DirEdge{};
	p_region->add_free_edge_gpos(Vector2i(0, 1), p_region->g_size, diredge);

	CHECK(diredge[Region::Direction::UP].size() == 1);
	CHECK(diredge[Region::Direction::DOWN].size() == 0);
	CHECK(diredge[Region::Direction::LEFT].size() == 0);
	CHECK(diredge[Region::Direction::RIGHT].size() == 0);

	CHECK(diredge[Region::Direction::UP][0].gpos == Vector2i{0, 0});
	CHECK(diredge[Region::Direction::UP][0].size == p_region->g_size);
}

TEST_CASE("[Region] no edge is added when grid position is outside of segment boundary < 0") {
	diredge = Region::DirEdge{};
	p_region->add_free_edge_gpos(Vector2i(0, 0), p_region->g_size, diredge);

	CHECK(diredge[Region::Direction::UP].size() == 0);
	CHECK(diredge[Region::Direction::DOWN].size() == 0);
	CHECK(diredge[Region::Direction::LEFT].size() == 0);
	CHECK(diredge[Region::Direction::RIGHT].size() == 0);
}

TEST_CASE("[Region] no edge is added when grid position is outside of segment boundary > grid_size") {
	diredge = Region::DirEdge{};
	s_region2->add_free_edge_gpos(
		grid_size - s_region2->g_size_inclusive,
		s_region2->g_size,
		diredge
	);

	if (diredge[Region::Direction::UP].size() != 1) {
		for (uint32_t i{ 0 }; i < diredge[Region::Direction::UP].size(); ++i) {
			MESSAGE("UP gpos ", i, ": ", diredge[Region::Direction::UP][i].gpos);
		}
	}
	CHECK(diredge[Region::Direction::UP].size() == 1);

	if (diredge[Region::Direction::DOWN].size() != 0) {
		for (uint32_t i{ 0 }; i < diredge[Region::Direction::DOWN].size(); ++i) {
			MESSAGE("DOWN gpos ", i, ": ", diredge[Region::Direction::DOWN][i].gpos);
		}
	}
	CHECK(diredge[Region::Direction::DOWN].size() == 0);

	if (diredge[Region::Direction::LEFT].size() != 1) {
		for (uint32_t i{ 0 }; i < diredge[Region::Direction::LEFT].size(); ++i) {
			MESSAGE("LEFT gpos ", i, ": ", diredge[Region::Direction::LEFT][i].gpos);
		}
	}
	CHECK(diredge[Region::Direction::LEFT].size() == 1);

	if (diredge[Region::Direction::RIGHT].size() != 0) {
		for (uint32_t i{ 0 }; i < diredge[Region::Direction::RIGHT].size(); ++i) {
			MESSAGE("RIGHT gpos ", i, ": ", diredge[Region::Direction::RIGHT][i].gpos);
		}
	}
	CHECK(diredge[Region::Direction::RIGHT].size() == 0);

	CHECK(diredge[Region::Direction::UP][0].gpos == Vector2i{
		grid_size.x - s_region2->g_size_inclusive.x,
		grid_size.y - s_region2->g_size_inclusive.y - 1
	});

	CHECK(diredge[Region::Direction::LEFT][0].gpos == Vector2i{
		grid_size.x - s_region2->g_size_inclusive.x - 1,
		grid_size.y - s_region2->g_size_inclusive.y
	});
}

TEST_CASE("[Region] fill_blocked_edges sets correct positions in generative occupancy") {
	Ref<PCG> pcg = PCG::create(grid_size, 0, false);

	Vector2i g_size{ p_region_full_blocked->g_size };
	p_region_full_blocked->fill_blocked_edges(Vector2i(1, 1), g_size, rng, pcg);

	// for 3x3 region
	std::array<LocalVector<Vector2i>, 4> blocked_gpos{
		LocalVector{ Vector2i{ 1, 0 }, { 2, 0 }, { 3, 0 } }, // up
		LocalVector{ Vector2i{ 1, 4 }, { 2, 4 }, { 3, 4 } }, // down
		LocalVector{ Vector2i{ 0, 1 }, { 0, 2 }, { 0, 3 } }, // left
		LocalVector{ Vector2i{ 4, 1 }, { 4, 2 }, { 4, 3 } }  // right
	};

	LocalVector<Vector2i> blocked_corner_gpos{
		Vector2i{ 0, 0 }, { 4, 0 }, { 4, 4 }, { 0, 4 }
	};

	std::array<String, 4> subcase_strings{ "up", "down", "left", "right" };

	for (int x{ 0 }; x < grid_size.x; ++x) {
		for (int y{ 0 }; y < grid_size.y; ++y) {
			const Vector2i gpos{ x, y };
			bool should_be_set{ false };
			String subcase_string{ "none" };

			if (blocked_corner_gpos.has(gpos)) {
				continue; // it is random whether it consumes the cell
			}
			for (int i{ 0 }; i < 4; ++i) {
				if (blocked_gpos[i].has(gpos)) {
					subcase_string = subcase_strings[i];
					should_be_set = true;
					break;
				}
				
			}

			SUBCASE(vformat("gpos %s for expected side %s (g_size: %s) is set", gpos, subcase_string, g_size).utf8().get_data()) {
				CHECK(pcg->generative_occupancy->is_gpos_set(gpos) == should_be_set);
			}
		}
	}
}

TEST_CASE("[Region] adding a region sets the occupancy correctly") {

	Ref<PCG> pcg = PCG::create(grid_size, 0, false);

	LocalVector<Vector2i> set_gpos{ Vector2i{ 0, 0 }, { 0, 1 } };

	diredge = Region::DirEdge{};
	p_region1->add_region(
		rng,
		pcg,
		Vector2i(0, 0),
		p_region1->g_size,
		p_region1->g_size_inclusive,
		diredge,
		0
	);

	for (int x{ 0 }; x < grid_size.x; ++x) {
		for (int y{ 0 }; y < grid_size.y; ++y) {
			const Vector2i gpos{ x, y };
			bool should_be_set{ false };

			if (set_gpos.has(gpos)) {
				should_be_set = true;
			}

			SUBCASE(vformat("gpos %s", gpos).utf8().get_data()) {
				CHECK(pcg->generative_occupancy->is_gpos_set(gpos) == should_be_set);
			}
		}
	}
	
		
	pcg->clear_occupancy();
}

TEST_CASE("[Region] out of bounds region does not set occupancy") {
	Ref<PCG> pcg = PCG::create(grid_size, 0, false);

	diredge = Region::DirEdge{};

	// fully out of bounds
	p_region1->add_region(
		rng,
		pcg,
		Vector2i(grid_size.x, grid_size.y),
		p_region1->g_size,
		p_region1->g_size_inclusive,
		diredge,
		0
	);

	for (int x{ 0 }; x < grid_size.x; ++x) {
		for (int y{ 0 }; y < grid_size.y; ++y) {
			const Vector2i gpos{ x, y };

			SUBCASE(vformat("fully out of bounds, gpos %s", gpos).utf8().get_data()) {
				CHECK(pcg->generative_occupancy->is_gpos_set(gpos) == false);
			}
		}
	}

	// blocked y side out of bounds
	p_region1->add_region(
		rng,
		pcg,
		Vector2i(grid_size.x, grid_size.y - 1),
		p_region1->g_size,
		p_region1->g_size_inclusive,
		diredge,
		0
	);

	for (int x{ 0 }; x < grid_size.x; ++x) {
		for (int y{ 0 }; y < grid_size.y; ++y) {
			const Vector2i gpos{ x, y };

			SUBCASE(vformat("blocked y side out of bounds, gpos %s", gpos).utf8().get_data()) {
				CHECK(pcg->generative_occupancy->is_gpos_set(gpos) == false);
			}
		}
	}
	
	pcg->clear_occupancy();
}

}



