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
#include "input.h"
#include "game.h"
#include "timer.h"
#include "gltf.h"

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

int main(int argc, char** argv)
{
	Platform platform;
	platform.init_window(WINDOW_WIDTH, WINDOW_HEIGHT, "GigaRay");
	Vk_Context ctx(&platform);
	Resource_Manager<Mesh> mesh_manager;
	Resource_Manager<Texture> texture_manager;
	Resource_Manager<Material> material_manager;
	Renderer renderer(&ctx, &platform, &mesh_manager, &texture_manager, &material_manager);

	//Mesh2 gltf = load_gltf_from_file("data/cube/Cube.gltf", &ctx, &texture_manager, &material_manager);
	Mesh2 gltf = load_gltf_from_file("data/sponza/Sponza.gltf", &ctx, &texture_manager, &material_manager);
	std::vector<Mesh> meshes(gltf.meshes.size());
	create_from_mesh2(&gltf, (u32)gltf.meshes.size(), meshes.data());

	//int32_t mesh_id = mesh_manager.load_from_disk("data/stanford-bunny.obj");
	//int32_t mesh_id = mesh_manager.load_from_disk("data/sponza.obj"); 
	//i32 mesh_id = mesh_manager.register_resource(meshes[30], "cube");
	ECS ecs{};
	Transform_Component c{};
	c.pos = glm::vec3(0.0, 2.2, 0.0);
	Camera_Component cam{};
	cam.proj = glm::perspective(glm::radians(75.f), (float)WINDOW_WIDTH/(float)WINDOW_HEIGHT, 0.1f, 1000.f);
	cam.view = glm::lookAt(glm::vec3(0.f, 0.1f, 0.15), glm::vec3(0.f, 0.1f, 0.f), glm::vec3(0.f, 1.f, 0.f));
	//Static_Mesh_Component m{&mesh_manager, mesh_id};
	Renderable_Component r{ &renderer, false};

	Game_State game_state;
	game_state.ecs = &ecs;

	for (size_t i = 0; i < meshes.size(); ++i)
	{
		i32 mesh_id = mesh_manager.register_resource(meshes[i], std::to_string(i));
		Static_Mesh_Component meshcomp = { &mesh_manager, mesh_id };
		ecs.add_entity(Transform_Component(), meshcomp);
	}
	//ecs.add_entity(Transform_Component(), m, r);
	//ecs.add_entity(c, m, r);
	game_state.player_entity = ecs.add_entity(cam, Transform_Component(), Velocity_Component());
	ecs.get_component<Transform_Component>(game_state.player_entity)->pos = glm::vec3(0.f, 0.1f, 0.15f);

	Timer timer;

	renderer.init_scene(&ecs);

	bool quit = false;
	while (!quit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				goto here;
			if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
			{
				if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
					goto here;
				if (!event.key.repeat)
					handle_key_event(event);
			}
			else if (event.type == SDL_MOUSEMOTION)
			{
				handle_mouse_event(event);
			}
		}

		float dt = timer.update();
		
		game_state.simulate(dt);
		renderer.pre_frame();
		renderer.begin_frame();
		renderer.draw(&ecs);
		renderer.end_frame();
	}
here:
	vkDeviceWaitIdle(ctx.device);
	g_garbage_collector->shutdown();
	return 0;
}