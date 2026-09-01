#pragma once

#include "core/object/ref_counted.h"

class Direction : public RefCounted {
	GDCLASS(Direction, RefCounted);

public:
	enum E { NONE = -1, UP, DOWN, LEFT, RIGHT, MAX };

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
};

VARIANT_ENUM_CAST(Direction::E)
