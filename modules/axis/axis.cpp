#include "axis.h"

void Axis::_bind_methods() {
	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(X);
	BIND_ENUM_CONSTANT(Y);
	BIND_ENUM_CONSTANT(ALL);
}

Direction::E Axis::mirror_direction(Axis::E axis, Direction::E direction) {
	if (
		(axis == Axis::Y && (direction == Direction::UP || direction == Direction::DOWN)) ||
		(axis == Axis::X && (direction == Direction::LEFT || direction == Direction::RIGHT)) ||
		axis == Axis::ALL
	) {
		return Direction::invert(direction);
	}
	return direction;
}

Vector2i Axis::mirror_alignment(Axis::E axis, Vector2i v) {
	if (axis == Axis::X) {
		v.x *= -1;
	}
	if (axis == Axis::Y) {
		v.y *= -1;
	}
	return v;
}
