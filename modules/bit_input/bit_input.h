#pragma once

#include "core/object/ref_counted.h"
#include <array>

class BitInput : public RefCounted {
	GDCLASS(BitInput, RefCounted);

public:
	enum Input {
		UP        = 0b1,
		DOWN      = 0b10,
		LEFT      = 0b100,
		RIGHT     = 0b1000,

		JUMP      = 0b10000,
		DASH      = 0b100000,
		DIG       = 0b1000000,
		MAX_INPUT = 0b10000000
	};

	enum GroupInput {
		LEFT_DASH  = LEFT | DASH,
		RIGHT_DASH = RIGHT | DASH,

		UP_DIG     = UP | DIG,
		DOWN_DIG   = DOWN | DIG,
		LEFT_DIG   = LEFT | DIG,
		RIGHT_DIG  = RIGHT | DIG,

		VERTICAL   = UP | DOWN,
		HORIZONTAL = LEFT | RIGHT,
		DIRECTION  = UP | DOWN | LEFT | RIGHT
	};

private:
	// these only keep the input, wiping all other inputs
	static constexpr std::array<int, 1> m_exclusive_bitmasks {
		
	};
	// these only keep the most recent key in the group pressed
	static constexpr std::array<int, 2> m_group_exclusive_bitmasks {
		UP | DOWN | LEFT | RIGHT, JUMP | DASH | DIG
	};
	static constexpr int m_persistent_inputs {
		LEFT | RIGHT
	};
	// for each memory input group we keep one previous state to return to
	static constexpr std::array<int, 1> m_memory_inputs_groups_bitmasks {
		GroupInput::HORIZONTAL
	};

	std::array<int, m_memory_inputs_groups_bitmasks.size()> inputs_memory{};

public:
	int inputs{ 0 };

private:
	void update_memory();

protected:
	static void _bind_methods();

public:
	int get_inputs() const;
	void wipe_memory();
	void set_inputs(int inputs);
	bool has_input(int input) const;
	bool has_all_group_input(int group_input) const;
	bool has_any_group_input(int group_input) const;
	void set_input(int input);
	void progress_frame();
	void release_inputs(int released_inputs);
};

VARIANT_ENUM_CAST(BitInput::Input);
VARIANT_ENUM_CAST(BitInput::GroupInput);
