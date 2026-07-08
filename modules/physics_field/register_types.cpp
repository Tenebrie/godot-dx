#include "register_types.h"

#include "core/object/class_db.h"

#include "physics_field.h"

void initialize_physics_field_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_ABSTRACT_CLASS(PhysicsFieldShape);
	GDREGISTER_CLASS(PhysicsFieldShapeRect);
	GDREGISTER_CLASS(PhysicsFieldShapeCircle);
	GDREGISTER_CLASS(PhysicsFieldObstacleSegment);
	GDREGISTER_CLASS(PhysicsFieldObstacle);
	GDREGISTER_CLASS(PhysicsFieldRaycastQuery);
	GDREGISTER_CLASS(PhysicsFieldRaycastResultHit);
	GDREGISTER_CLASS(PhysicsFieldRaycastResult);
	GDREGISTER_CLASS(PhysicsField);
}

void uninitialize_physics_field_module(ModuleInitializationLevel p_level) {
}
