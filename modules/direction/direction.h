#pragma once

#include "core/object/ref_counted.h"
#include <array>

class Direction : public RefCounted {
	GDCLASS(Direction, RefCounted);

public:
	enum E { NONE = -1, UP, DOWN, LEFT, RIGHT, MAX };

private:
	inline static const std::array<Vector2i, 4> direction_vector2i {
		Vector2i{ 0, 1 }, Vector2i{ 0, -1 }, Vector2i{ -1, 0 }, Vector2i{ 1, 0 }
	};

protected:
	static void _bind_methods();

public:
	static E invert(E direction) {
		switch (direction) {
			case E::UP:       return E::DOWN;
			case E::DOWN:     return E::UP;
			case E::LEFT:     return E::RIGHT;
			case E::RIGHT:    return E::LEFT;
			default:          return E::NONE;
		}
	}
	static Vector2i direction_to_vector2i(E direction);
};

VARIANT_ENUM_CAST(Direction::E)
