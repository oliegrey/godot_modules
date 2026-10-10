#include "bit_input.h"

void BitInput::_bind_methods() {
	ClassDB::bind_static_method(
		"BitInput", D_METHOD("max_input_bytes"), &BitInput::max_input_bytes
	);

	ClassDB::bind_method(D_METHOD("get_inputs"), &BitInput::get_inputs);
	ClassDB::bind_method(D_METHOD("set_inputs", "new_inputs"), &BitInput::set_inputs);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "inputs"), "", "get_inputs");

	ClassDB::bind_method(D_METHOD("has_transient_inputs"), &BitInput::has_transient_inputs);
	ClassDB::bind_method(D_METHOD("clear_inputs"), &BitInput::clear_inputs);
	ClassDB::bind_method(
		D_METHOD("has_any_input", "new_inputs"), &BitInput::has_any_input
	);
	ClassDB::bind_method(
		D_METHOD("has_all_inputs", "new_inputs"), &BitInput::has_all_inputs
	);
	ClassDB::bind_method(D_METHOD("set_input", "new_input"), &BitInput::set_input);
	ClassDB::bind_method(D_METHOD("progress_frame"), &BitInput::progress_frame);
	ClassDB::bind_method(
		D_METHOD("release_inputs", "released_inputs"), &BitInput::release_inputs
	);

	BIND_ENUM_CONSTANT(UP);
	BIND_ENUM_CONSTANT(DOWN);
	BIND_ENUM_CONSTANT(LEFT);
	BIND_ENUM_CONSTANT(RIGHT);
	BIND_ENUM_CONSTANT(JUMP);
	BIND_ENUM_CONSTANT(DASH);
	BIND_ENUM_CONSTANT(DIG);

	BIND_ENUM_CONSTANT(LEFT_DASH);
	BIND_ENUM_CONSTANT(RIGHT_DASH);
	BIND_ENUM_CONSTANT(VERTICAL);
	BIND_ENUM_CONSTANT(HORIZONTAL);
	BIND_ENUM_CONSTANT(DIRECTION);
}

	
int BitInput::max_input_bytes() {
	return 1 + (INPUT_MAX - 1 > 0xFF) + (INPUT_MAX - 1 > 0xFFFF) + (INPUT_MAX - 1 > 0xFFFFFF);
}

int BitInput::get_inputs() const {
	return inputs;
}

void BitInput::set_inputs(int new_inputs) {
	inputs = new_inputs;
}

void BitInput::clear_inputs() {
	inputs = 0;
	m_persistent_memory.fill(0);
}

bool BitInput::has_any_input(int new_inputs) const {
	return static_cast<bool>(inputs & new_inputs);
}

bool BitInput::has_all_inputs(int new_inputs) const {
	return (inputs & new_inputs) == new_inputs;
}

bool BitInput::has_transient_inputs() const {
	return static_cast<bool>(inputs & ~m_all_persistent_inputs);
}

// Single inputs only.
void BitInput::set_input(int new_input) {
	// No change.
	if ((new_input | inputs) == inputs) {
		return;
	}

	// A persistent exclusive is set so ignore following inputs until released.
	for (int bitmask: m_exclusive) {
		if (inputs & bitmask) {
			return;
		}
	}

	// The input is exclusive so set to only this input and return.
	for (int bitmask: m_exclusive) {
		if (new_input & bitmask) {
			clear_inputs();
			inputs = new_input;
			return;
		}
	}

	if (new_input & m_all_persistent_inputs) {
		// Get the persistent memory before mutating.
		m_persistent_memory[m_persistent_memory_i] = inputs & m_all_persistent_inputs;
		m_persistent_memory_i = (m_persistent_memory_i + 1ll) % m_persistent_memory.size();
	}

	// The new input is a group exclusive so wipe the group.
	for (int bitmask: m_group_exclusive) {
		if (new_input & bitmask) {
			inputs &= ~bitmask;
		}
	}

	// Finally set the bit.
	inputs |= new_input;
}

void BitInput::progress_frame() {
	// Release everything except for the persistent inputs.
	// Every other input only lasts 1 frame per press.
	inputs &= m_all_persistent_inputs;
}

// Can be any combination or single input. Ensures memory is also released.
void BitInput::release_inputs(int released_inputs) {
	// Remove from current inputs.
	inputs &= ~released_inputs;

	// Ensure released key is not in memory.
	for (int i{ 0 }; i < m_persistent_memory.size(); ++i) {
		m_persistent_memory[i] &= ~released_inputs;
	}

	// If there is a persistent input in the released inputs add to current inputs.
	if (m_all_persistent_inputs & released_inputs) {

		const int size{ m_persistent_memory.size() };
		// The newest memory is one place behind as its already incremented.
		const int newest{ (m_persistent_memory_i + size - 1) % size };

		for (int i{ 0 }; i < size; ++i) {
			const int wrapped_i{ (newest + size - i) % size };

			if (m_persistent_memory[wrapped_i] != 0) {
				inputs |= m_persistent_memory[wrapped_i];
				break;
			}
		}
	}
}
