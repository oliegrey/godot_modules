#include "register_types.h"

#include "core/object/class_db.h"
#include "subgrid_probe.h"

void initialize_subgrid_probe_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ClassDB::register_abstract_class<SubgridProbe>();
}

void uninitialize_subgrid_probe_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
