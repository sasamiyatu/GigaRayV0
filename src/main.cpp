#define VK_NO_PROTOTYPES
#include "SDL.h"
#include "SDL_vulkan.h"
#define VOLK_IMPLEMENTATION
#include "volk/volk.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <assert.h>
#include <vector>
#include <array>
#include "renderer.h"
#include "vk_helpers.h"
#include "obj.h"
#include "ecs.h"
#include "resource_manager.h"
#include "r_mesh.h"
#include <any>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

int main(int argc, char** argv)
{
	Platform platform;
	platform.init_window(WINDOW_WIDTH, WINDOW_HEIGHT, "GigaRay");
	Vk_Context ctx(&platform);
	Renderer renderer(&ctx, &platform);

	Resource_Manager<Mesh> mesh_manager;
	int32_t mesh_id = mesh_manager.load_from_disk("data/stanford-bunny.obj");
	ECS ecs{};
	Transform_Component c{};
	Static_Mesh_Component m{&mesh_manager, mesh_id};
	Renderable_Component r{ &renderer, false};

	ecs.add_entity(c, m, r);

	Rt_Scene scene;
	scene.initialize_renderables(&ecs);
	Scene test_scene{};

	Static_Mesh_Component* mesh_comp = ecs.get_component<Static_Mesh_Component>(mesh_id);
	Mesh* mesh = mesh_comp->manager->get_resource_with_id(mesh_comp->mesh_id);
	Vk_Acceleration_Structure tlas = renderer.vk_create_top_level_acceleration_structure(mesh, renderer.get_current_frame_command_buffer());
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