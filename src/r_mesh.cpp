#include "r_mesh.h"
#include "resource_manager.h"

Mesh* get_mesh(ECS* ecs, uint32_t entity_id)
{
	Static_Mesh_Component* mesh = ecs->get_component<Static_Mesh_Component>(entity_id);
	if (!mesh) return nullptr;

	Mesh* m = mesh->manager->get_resource_with_id(mesh->mesh_id);
	if (!m) return nullptr;

	return m;
}
