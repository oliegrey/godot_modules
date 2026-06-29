#pragma once

#include "core/object/ref_counted.h"
#include <array>

class Region : public RefCounted {
	GDCLASS(Region, RefCounted);

public:
	enum Slot { PRIMARY, SECONDARY };
	enum Direction { NONE = -1, UP, DOWN, LEFT, RIGHT };

private:
	inline static Vector2i m_max_g_size;

	// attachment direction [0 - 3]
	// -> quad size flattened [(0, 0), ... (max, 0), ... (max, max)]
	// -> region
	// to find a region that fits after a failed test with free space
	// order quad size from smallest to largest so we can find the next lowest
	// region vector ordered by min world segment, can iterate to find the max
	inline static std::array<LocalVector<LocalVector<Ref<Region>>>, 4> m_region_tree;

	// attachment direction (already placed regions perspective) [0 - 3] -> region
	// to get a random region to test in a free direction
	// region vector ordered by min world segment, can iterate to find the max
	inline static std::array<LocalVector<Ref<Region>>, 4> m_dir_to_region;

	// starting regions to build off of
	inline static LocalVector<Ref<Region>> m_primary_regions;

	int m_flat_size_i;

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

protected:
	static void _bind_methods();

public:
	static void initialize(Vector2i max_g_size);

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
};

VARIANT_ENUM_CAST(Region::Slot)
VARIANT_ENUM_CAST(Region::Direction)
