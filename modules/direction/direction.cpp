#include "direction.h"

void Direction::_bind_methods() {
	ClassDB::bind_static_method(
		"Direction", D_METHOD("to_vector2i", "direction"),
		&Direction::to_vector2i
	);

	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(UP);
	BIND_ENUM_CONSTANT(DOWN);
	BIND_ENUM_CONSTANT(LEFT);
	BIND_ENUM_CONSTANT(RIGHT);
	BIND_ENUM_CONSTANT(MAX);
}

Vector2i Direction::to_vector2i(E direction) {
	return direction_vector2i[direction];
}
