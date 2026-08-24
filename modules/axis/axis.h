#pragma once

#include "core/object/ref_counted.h"
#include "modules/direction/direction.h"

class Axis : public RefCounted {
	GDCLASS(Axis, RefCounted);

public:
	enum E { NONE, X, Y, ALL };

protected:
	void _bind_methods();

public:
	static Direction::E mirror_direction(Axis::E axis, Direction::E direction);
};

VARIANT_ENUM_CAST(Axis::E)
