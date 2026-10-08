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

void BitInput::set_inputs(int _inputs) {
	inputs = _inputs;
}

bool BitInput::has_input(int input) const {
	return static_cast<bool>(inputs & input);
}

bool BitInput::has_all_group_input(int group_input) const {
	return (inputs & group_input) == group_input;
}bool BitInput::has_any_group_input(int group_input) const {
	return static_cast<bool>(inputs & group_input);
}

void BitInput::set_input(int input) {
	// an exclusive is set so ignore following inputs
	for (int bitmask: m_exclusive_bitmasks) {
		if (inputs & bitmask) {
			return;
		}
	}

	// the input is exclusive so set to only this input and return
	for (int bitmask: m_exclusive_bitmasks) {
		if (input & bitmask) {
			inputs = input;
			return;
		}
	}

	// the input is a group exclusive so wipe the group
	for (int bitmask: m_group_exclusive_bitmasks) {
		if (input & bitmask) {
			inputs &= ~bitmask;
		}
	}

	// finally set the bit
	inputs |= input;
}

void BitInput::progress_frame() {
	// release everything except the held inputs
	inputs &= m_held_inputs_bitmask;
}

void BitInput::clear_input(int input) {
	inputs &= ~input;
}

void BitInput::clear_group(int group_input) {
	inputs &= ~group_input ;
}
