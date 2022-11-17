#define VK_NO_PROTOTYPES
#include "SDL.h"
#include "SDL_vulkan.h"
#define VOLK_IMPLEMENTATION
#include "Volk/volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
#include <assert.h>
#include <vector>
#include <array>
#include "renderer.h"
#include "vk_helpers.h"
#include "obj.h"
#include "ecs.h"
#include "resource_manager.h"
#include "r_mesh.h"

int main(int argc, char** argv)
{

	Renderer renderer;
	renderer.initialize();

	Resource_Manager<Mesh> mesh_manager;
	int32_t mesh_id = mesh_manager.load_from_disk("data/stanford-bunny.obj");
	ECS ecs{};
	Transform_Component c{};
	Static_Mesh_Component m{&mesh_manager, mesh_id};
	Renderable_Component r{ &renderer, false};

	ecs.add_entity(c, m, r);

	//Mesh bunny_mesh = load_obj_from_file("data/stanford-bunny.obj");
	Rt_Scene scene;
	scene.initialize_renderables(&ecs);
	Scene test_scene{};
	//Mesh test_mesh = load_obj_from_file("data/stanford-bunny.obj");
	//test_mesh.vertices = { glm::vec3(-0.5, -0.5, 0.f), glm::vec3(0.5, -.5f, 0.f), glm::vec3(0.f, .5f, 0.f) };
	//test_mesh.indices = { 0, 1, 2 };
	//test_mesh.create_vertex_buffer(&renderer);
	//test_mesh.create_index_buffer(&renderer);

	//test_mesh.build_bottom_level_acceleration_structure();
	Static_Mesh_Component* mesh_comp = ecs.get_component<Static_Mesh_Component>(mesh_id);
	Mesh* mesh = mesh_comp->manager->get_resource_with_id(mesh_comp->mesh_id);
	//Vk_Acceleration_Structure tlas = renderer.vk_create_top_level_acceleration_structure(&test_mesh);
	Vk_Acceleration_Structure tlas = renderer.vk_create_top_level_acceleration_structure(mesh);

	//test_scene.meshes = { std::move(test_mesh) };
	//test_scene.instances = { {glm::mat4(1.0)} };
	test_scene.tlas = std::move(tlas);
	renderer.scene = std::move(test_scene);

	bool quit = false;
	while (!quit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				goto here;

		}
		
		renderer.begin_frame();
		renderer.draw(&ecs);
		renderer.end_frame();
	}
here:
	
	return 0;
}