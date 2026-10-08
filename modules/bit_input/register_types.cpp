#include "register_types.h"

#include "core/object/class_db.h"
#include "bit_input.h"

void initialize_bit_input_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(BitInput);
	}
}

void uninitialize_bit_input_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
