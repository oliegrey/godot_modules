#include "bit_input.h"

void BitInput::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_inputs"), &BitInput::get_inputs);
	ClassDB::bind_method(D_METHOD("set_inputs", "inputs"), &BitInput::set_inputs);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "inputs"), "set_inputs", "get_inputs");

	ClassDB::bind_method(D_METHOD("has_input", "input"), &BitInput::has_input);
	ClassDB::bind_method(
		D_METHOD("has_all_group_input", "group_input"), &BitInput::has_all_group_input
	);
	ClassDB::bind_method(
		D_METHOD("has_any_group_input", "group_input"), &BitInput::has_any_group_input
	);
	ClassDB::bind_method(D_METHOD("set_input", "input"), &BitInput::set_input);
	ClassDB::bind_method(D_METHOD("progress_frame"), &BitInput::progress_frame);
	ClassDB::bind_method(D_METHOD("clear_input", "input"), &BitInput::clear_input);
	ClassDB::bind_method(D_METHOD("clear_group", "group_input"), &BitInput::clear_group);

	BIND_ENUM_CONSTANT(LEFT);
	BIND_ENUM_CONSTANT(RIGHT);
	BIND_ENUM_CONSTANT(UP);
	BIND_ENUM_CONSTANT(DOWN);
	BIND_ENUM_CONSTANT(DASH);

	BIND_ENUM_CONSTANT(LEFT_DASH);
	BIND_ENUM_CONSTANT(RIGHT_DASH);
	BIND_ENUM_CONSTANT(VERTICAL);
	BIND_ENUM_CONSTANT(HORIZONTAL);
	BIND_ENUM_CONSTANT(DIRECTION);
}

int BitInput::get_inputs() const {
	return inputs;
}

void BitInput::wipe_memory() {
	for (int i{ 0 }; i < inputs_memory.size(); ++ i) {
		inputs_memory[i] = 0;
	}
}

// wipes memory of previous inputs
void BitInput::set_inputs(int _inputs) {
	wipe_memory();
	inputs = _inputs;
}

bool BitInput::has_input(int input) const {
	return static_cast<bool>(inputs & input);
}

bool BitInput::has_all_group_input(int group_input) const {
	return (inputs & group_input) == group_input;
}

bool BitInput::has_any_group_input(int group_input) const {
	return static_cast<bool>(inputs & group_input);
}

void BitInput::set_input(int input) {
	if ((input | inputs) == inputs) {
		return;
	}

	// An exclusive is set so ignore following inputs until unset.
	for (int bitmask: m_exclusive_bitmasks) {
		if (inputs & bitmask) {
			return;
		}
	}

	update_memory();

	// The input is exclusive so set to only this input and return.
	for (int bitmask: m_exclusive_bitmasks) {
		if (input & bitmask) {
			inputs = input;
			return;
		}
	}

	// The input is a group exclusive so wipe the group.
	for (int bitmask: m_group_exclusive_bitmasks) {
		if (input & bitmask) {
			inputs &= ~bitmask;
		}
	}

	// Finally set the bit.
	inputs |= input;
}

// Whenever any input is received we check if the inputs about to be changed
// differs from memory groups. Ensure this is called BEFORE changes to inputs.
void BitInput::update_memory() {
	for (int i{ 0 }; i < m_memory_inputs_groups_bitmasks.size(); ++i) {
		int group_bitmap{ inputs & m_memory_inputs_groups_bitmasks[i] };

		if (group_bitmap != inputs_memory[i]) {
			inputs_memory[i] = group_bitmap;
		}
	}
}

void BitInput::progress_frame() {
	// Release everything except the persistent inputs.
	// Every other input only lasts 1 frame per press.
	inputs &= m_persistent_inputs;
}

// can be any combination or single input. Ensures memory is also released.
void BitInput::release_inputs(int released_inputs) {

	// Revert to previous for each group that exists in released_inputs.
	// Clear from released_inputs so the final release does not wipe this step.
	for (int i{ 0 }; i < m_memory_inputs_groups_bitmasks.size(); ++i) {
		int bitmask{ m_memory_inputs_groups_bitmasks[i] };

		// If any inputs present from a memory group...
		// In current inputs: wipe the group, set current to memory, then clear released from memory
		// Only in memory: clear only released from memory
		if (released_inputs & bitmask) {
			int input_group_memory{ inputs_memory[i] & released_inputs };
			if (inputs & bitmask) {
				inputs &= ~m_memory_inputs_groups_bitmasks[i];
				inputs |= input_group_memory;
				inputs_memory[i] &= ~released_inputs;
			} else if (inputs_memory[i] & bitmask) {
				inputs_memory[i] &= ~released_inputs;
			}

			released_inputs &= ~bitmask;
		}
	}

	// Release only non memory group inputs.
	inputs &= ~released_inputs;
}
