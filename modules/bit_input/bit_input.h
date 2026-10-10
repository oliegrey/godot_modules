#pragma once

#include "core/object/ref_counted.h"
#include <array>

namespace BitCombine {
	template <size_t N>
	static constexpr int or_all(const std::array<int, N> &bitmasks) {
		int combined_bitmask{ 0 };
		for (int bitmask: bitmasks) {
			combined_bitmask |= bitmask;
		}
		return combined_bitmask;
	}
}

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
		INTERACT  = 0b10000000,
		INPUT_MAX = 0b100000000
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
	// Only keeps the exclusive input when any input received (wipes persistent).
	static constexpr std::array<int, 1> m_exclusive{
		INTERACT
	};
	static constexpr int m_all_exclusive{
		BitCombine::or_all<m_exclusive.size()>(m_exclusive)
	};

	// Carried over per frame until manually released.
	static constexpr std::array<int, 4> m_persistent_inputs{
		UP, DOWN, LEFT, RIGHT
	};
	static constexpr int m_all_persistent_inputs{
		BitCombine::or_all<m_persistent_inputs.size()>(m_persistent_inputs)
	};

	// The most recent key in the group pressed when input received.
	static constexpr std::array<int, 3> m_group_exclusive{
		UP | DOWN | LEFT | RIGHT, JUMP | DASH | DIG
	};
	static constexpr int m_all_group_exclusive{
		BitCombine::or_all<m_group_exclusive.size()>(m_group_exclusive)
	};

	inline static std::array<int, 8> m_persistent_memory{};
	inline static int m_persistent_memory_i{ 0 };

public:
	int inputs{ 0 };

protected:
	static void _bind_methods();

public:
	static int max_input_bytes();
	int get_inputs() const;
	void set_inputs(int new_inputs);
	void clear_inputs();
	bool has_any_input(int new_inputs) const;
	bool has_all_inputs(int new_inputs) const;
	bool has_transient_inputs() const;
	void set_input(int input);
	void progress_frame();
	void release_inputs(int released_inputs);
};

VARIANT_ENUM_CAST(BitInput::Input);
VARIANT_ENUM_CAST(BitInput::GroupInput);
