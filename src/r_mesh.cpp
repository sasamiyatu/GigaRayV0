#include "r_mesh.h"
#include "resource_manager.h"

void Rt_Scene::initialize_renderables(ECS* ecs)
{
	auto func = [&](Renderable_Component comp, uint32_t entity_id)
	{
		if (comp.render_ready == true) return;

		Renderer* r = comp.renderer;
		Static_Mesh_Component* mesh_component = ecs->get_component<Static_Mesh_Component>(entity_id);
		Mesh* mesh = mesh_component->manager->get_resource_with_id(mesh_component->mesh_id);
		mesh->renderer = r;
		mesh->create_vertex_buffer(r);
		mesh->create_index_buffer(r);
		mesh->build_bottom_level_acceleration_structure();
	};
	ecs->iterate<Renderable_Component>(func);
}

void Rt_Scene::build_top_level_acceleration_structure(ECS* ecs)
{
	// FIXME: This should collect all the meshes in the scene, but right now there's only one
	
}

Mesh* get_mesh(ECS* ecs, uint32_t entity_id)
{
	Static_Mesh_Component* mesh = ecs->get_component<Static_Mesh_Component>(entity_id);
	if (!mesh) return nullptr;

	Mesh* m = mesh->manager->get_resource_with_id(mesh->mesh_id);
	if (!m) return nullptr;

	return m;
}
