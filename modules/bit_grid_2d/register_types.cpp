#include "register_types.h"

#include "core/object/class_db.h"
#include "bit_grid_2d.h"

void initialize_bit_grid_2d_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ClassDB::register_abstract_class<BitGrid2D>();
}

void uninitialize_bit_grid_2d_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
