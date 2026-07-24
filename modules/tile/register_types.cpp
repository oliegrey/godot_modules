#include "register_types.h"

#include "core/object/class_db.h"
#include "tile.h"

void initialize_tile_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ClassDB::register_abstract_class<Tile>();
	Tile::init_layer_configs();
}

void uninitialize_tile_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
