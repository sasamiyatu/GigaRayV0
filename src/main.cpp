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
#include "windows.h"
#include "renderer.h"
#include "vk_helpers.h"
#include "obj.h"

int main(int argc, char** argv)
{

	Renderer renderer;
	renderer.initialize();

	//Mesh bunny_mesh = load_obj_from_file("data/stanford-bunny.obj");

	Scene test_scene{};
	Mesh test_mesh = load_obj_from_file("data/stanford-bunny.obj");
	test_mesh.renderer = &renderer;
	//test_mesh.vertices = { glm::vec3(-0.5, -0.5, 0.f), glm::vec3(0.5, -.5f, 0.f), glm::vec3(0.f, .5f, 0.f) };
	//test_mesh.indices = { 0, 1, 2 };
	test_mesh.create_vertex_buffer(&renderer);
	test_mesh.create_index_buffer(&renderer);

	test_mesh.build_bottom_level_acceleration_structure();


	VkAccelerationStructureInstanceKHR instance = vkinit::acceleration_structure_instance(test_mesh.blas.value().acceleration_structure_buffer_address);

	Vk_Allocated_Buffer buffer_instances = renderer.vk_allocate_buffer((uint32_t)sizeof(instance), 
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
	renderer.vk_upload_cpu_to_gpu(buffer_instances.buffer, &instance, (uint32_t)sizeof(instance));

	Vk_Acceleration_Structure tlas = renderer.vk_create_top_level_acceleration_structure(&buffer_instances, 1);

	test_scene.meshes = { std::move(test_mesh) };
	test_scene.instances = { {glm::mat4(1.0)} };
	test_scene.tlas = std::move(tlas);
	renderer.scene = std::move(test_scene);

	bool quit = false;
	while (!quit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				quit = true;

		}
		
		renderer.begin_frame();
		renderer.draw();
		renderer.end_frame();
	}
	
	return 0;
}