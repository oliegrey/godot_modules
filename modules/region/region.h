#pragma once

#include "core/object/ref_counted.h"

#include <array>

class Region;
class RandomNumberGenerator;
class PCG;

using RegionVector = LocalVector<Ref<Region>>;

class Region : public RefCounted {
	GDCLASS(Region, RefCounted);

public:
	enum Slot { PRIMARY, SECONDARY };
	enum Direction {
		NONE = -1, UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3, MAX = 4
	};

private:
	inline static const Vector2i MAX_G_SIZE{ Vector2i(8, 8) };
	inline static const int MAX_CELL_COUNT{ 64 };
	inline static const int FLAT_TREE_SIZE{ MAX_CELL_COUNT * Direction::MAX };

	inline static Vector2i m_seg_g_size;
	inline static int m_seg_cell_count;

	// index = attachment Direction [0 - 3] * quad size flattened i.e. (4 * 3)
	// -> all secondary regions that fit size and direction
	// RegionVector ordered by threshold so its easy to iterate within bounds
	inline static std::array<RegionVector, FLAT_TREE_SIZE> m_region_tree;
	// bitmap to find the next size that has matching regions
	inline static std::array<uint64_t, Direction::MAX> m_region_tree_occ;

	// attachment direction (already placed regions perspective) [0 - 3] -> region
	// to get a random region to test in a free direction
	// RegionVector ordered by threshold so its easy to iterate within bounds
	inline static std::array<RegionVector, Direction::MAX> m_dir_to_region;

	// starting regions to build off of
	// ordered by threshold so its easy to iterate within bounds
	inline static RegionVector m_primary_regions;
	// ordered by threshold so its easy to iterate within bounds
	inline static RegionVector m_secondary_regions;

	int m_cell_count;

public:
	String name;
	Slot slot;
	Vector2i g_size;
	PackedInt32Array blocked_sides;
	PackedInt32Array joining_sides;
	float spawn_weight;
	int threshold;
	Vector2i g_size_inclusive; // includes stone sides

private:
	static Direction invert_direction(Direction direction) {
		switch (direction) {
			case Direction::UP:       return Direction::DOWN;
			case Direction::DOWN:     return Direction::UP;
			case Direction::LEFT:     return Direction::RIGHT;
			case Direction::RIGHT:    return Direction::LEFT;
			default:                  return Direction::NONE;
		}
	}

	static int get_next_set_bit(uint64_t bitmap, const int start_i);

protected:
	static void _bind_methods();

public:
	static void initialize(Vector2i seg_g_size);

	static Ref<Region> create(
		String _name,
		Slot _slot,
		Vector2i _size,
		PackedInt32Array _blocked_sides,
		PackedInt32Array _joining_sides,
		int _spawn_weight,
		int _threshold
	);

	static void finalize();

	static PackedFloat32Array get_normalized_weights(
		const LocalVector<Ref<Region>> &regions, const int regions_max
	);

	static void generate_zone(
		Ref<RandomNumberGenerator> rng,
		Ref<PCG> pcg,
		const int w_seg,
		const int max_secondary_count
	);
};

VARIANT_ENUM_CAST(Region::Slot)
VARIANT_ENUM_CAST(Region::Direction)
