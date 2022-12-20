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
	Mesh combined_mesh{};
	std::vector<Indirect_Draw_Data> draw_data = merge_meshes((u32)meshes.size(), meshes.data(), &combined_mesh);
	//std::vector<Mesh> meshes;
	Material test_mat;
	test_mat.base_color_factor = glm::vec4(0.95, 0.93, 0.88, 1.0);
	test_mat.metallic_factor = 1.0f;
	test_mat.roughness_factor = 0.25f;
	i32 material_id = material_manager.register_resource(test_mat, "test");
	//meshes.push_back(create_sphere(16));
	//meshes[0].material_id = material_id;

	ECS ecs{};
	Transform_Component c{};
	c.pos = glm::vec3(0.0, 2.2, 0.0);
	Camera_Component cam{};
	cam.fov = 75.f;
	Renderable_Component r{ &renderer, false};

	Game_State game_state;
	game_state.ecs = &ecs;

#if 1
	for (size_t i = 0; i < meshes.size(); ++i)
	{
		i32 mesh_id = mesh_manager.register_resource(meshes[i], std::to_string(i));
		Static_Mesh_Component meshcomp = { &mesh_manager, mesh_id };
		ecs.add_entity(Transform_Component(), meshcomp);
	}
#else
	{
		i32 mesh_id = mesh_manager.register_resource(combined_mesh, "combined");
		Static_Mesh_Component meshcomp = { &mesh_manager, mesh_id };
		ecs.add_entity(Transform_Component(), meshcomp);
	}
#endif
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
				if (event.key.keysym.scancode == SDL_SCANCODE_F1 && !event.key.repeat && event.type == SDL_KEYDOWN)
				{
					if (renderer.render_mode == Renderer::PATH_TRACER)
						renderer.render_mode = Renderer::RASTER;
					else
						renderer.render_mode = Renderer::PATH_TRACER;

					renderer.frames_accumulated = 0;
				}
				if (event.key.keysym.scancode == SDL_SCANCODE_F2 && !event.key.repeat && event.type == SDL_KEYDOWN)
					renderer.render_mode = Renderer::SIDE_BY_SIDE;
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
	renderer.cleanup();
	g_garbage_collector->shutdown();

	return 0;
}