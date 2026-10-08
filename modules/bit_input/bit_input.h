#pragma once

#include "core/object/ref_counted.h"
#include <array>

class BitInput : public RefCounted {
	GDCLASS(BitInput, RefCounted);

public:
	enum Input {
		LEFT  = 0b1,
		RIGHT = 0b10,
		UP    = 0b100,
		DOWN  = 0b1000,
		DASH  = 0b10000
	};
	enum GroupInput {
		LEFT_DASH  = LEFT | DASH,
		RIGHT_DASH = RIGHT | DASH,
		VERTICAL   = UP | DOWN,
		HORIZONTAL = LEFT | RIGHT,
		DIRECTION  = UP | DOWN | LEFT | RIGHT
	};

private:
	static constexpr std::array<int, 1> m_exclusive_bitmasks {
		DOWN
	};
	static constexpr std::array<int, 2> m_group_exclusive_bitmasks {
		LEFT | RIGHT, DASH | UP | DOWN
	};
	static constexpr int m_held_inputs_bitmask {
		LEFT | RIGHT
	};

public:
	int inputs{ 0 };

protected:
	static void _bind_methods();

public:
	int get_inputs() const;
	void set_inputs(int inputs);
	bool has_input(int input) const;
	bool has_all_group_input(int group_input) const;
	bool has_any_group_input(int group_input) const;
	void set_input(int input);
	void progress_frame();
	void clear_input(int input);
	void clear_group(int group_input);
};

VARIANT_ENUM_CAST(BitInput::Input);
VARIANT_ENUM_CAST(BitInput::GroupInput);
