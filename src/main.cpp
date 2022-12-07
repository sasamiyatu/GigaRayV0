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
#include "shaders.h"
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

int main(int argc, char** argv)
{
	Timer timer;
	Platform platform;
	platform.init_window(WINDOW_WIDTH, WINDOW_HEIGHT, "GigaRay");
	Vk_Context ctx(&platform);
	Resource_Manager<Mesh> mesh_manager;
	Resource_Manager<Texture> texture_manager;
	Resource_Manager<Material> material_manager;
	Renderer renderer(&ctx, &platform, &mesh_manager, &texture_manager, &material_manager, &timer);


	//Mesh2 gltf = load_gltf_from_file("data/cube/Cube.gltf", &ctx, &texture_manager, &material_manager);
	Mesh2 gltf = load_gltf_from_file("data/sponza/Sponza.gltf", &ctx, &texture_manager, &material_manager);
	std::vector<Mesh> meshes(gltf.meshes.size());
	create_from_mesh2(&gltf, (u32)gltf.meshes.size(), meshes.data());

	ECS ecs{};
	Transform_Component c{};
	c.pos = glm::vec3(0.0, 2.2, 0.0);
	Camera_Component cam{};
	cam.fov = 75.f;
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
	{
		game_state.player_entity = ecs.add_entity(cam, Transform_Component(), Velocity_Component());
		auto* xform = ecs.get_component<Transform_Component>(game_state.player_entity);
		xform->pos = glm::vec3(0.f, 0.1f, 0.15f);
		ecs.get_component<Camera_Component>(game_state.player_entity)->set_transform(xform);
	}

	renderer.init_scene(&ecs);

	bool quit = false;
	while (!quit)
	{
		double start = timer.get_current_time();

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
		renderer.do_frame(&ecs);

		double end = timer.get_current_time();
	}
here:
	vkDeviceWaitIdle(ctx.device);
	g_garbage_collector->shutdown();
	return 0;
}