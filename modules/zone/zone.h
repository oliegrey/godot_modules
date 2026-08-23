#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/typed_array.h"
#include "modules/region/region.h"

#include <array>

class Region;
class RandomNumberGenerator;
class PCG;
class BitGrid2D;

class Zone : public RefCounted {
	GDCLASS(Zone, RefCounted);

private:
	inline static constexpr int FLAT_DIR_CELLS{ Region::MAX_CELL_COUNT * Region::DIRECTION_MAX };

	struct Edge { Vector2i gpos; Vector2i size; };

private:
	using DirEdge = std::array<LocalVector<Edge>, Region::DIRECTION_MAX>;
	using FlatDirSizeToGposArr = std::array<LocalVector<Vector2i>, FLAT_DIR_CELLS>;

	inline static std::array<std::array<uint64_t, 8>, 8> dominance_mask;
	inline static Vector2i m_seg_g_size;
	inline static int m_seg_cell_count;

	bool is_debug;
	Ref<RandomNumberGenerator> rng;
	Ref<PCG> pcg;


private:
	static void init_dominance_mask();

	static int get_size_or_larger_i(uint64_t bitmap, const Vector2i size);
	static int get_size_or_larger_i(uint64_t bitmap, const int cell_count);
	static int get_region_relative_size_i(Vector2i size);

protected:
	void _bind_methods();

public:
	Ref<Zone> create(
		Ref<RandomNumberGenerator> rng, Ref<PCG> pcg, Vector2i seg_g_size, bool is_debug
	);

	void fill_blocked_sides(Ref<Region> region, Rect2i region_rect);
};
