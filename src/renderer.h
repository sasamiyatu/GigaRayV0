#pragma once
#include "Volk/volk.h"
#include "vma/vk_mem_alloc.h"
#define VK_NO_PROTOTYPES
#include "SDL.h"
#include "SDL_vulkan.h"
#include <vector>
#include <array>
#include <string>
#include <optional>
#include "defines.h"
#include "r_vulkan.h"
#include "resource_manager.h"

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			printf("Detected Vulkan error: %d\n", err);			    \
			abort();                                                \
		}                                                           \
	} while (0)

struct ECS;


struct Vk_RenderTarget
{
	Vk_Allocated_Image image;
	VkFormat format;
	VkImageLayout layout;
	std::string name;
};

struct Vk_Framebuffer
{
	std::vector<Vk_RenderTarget> render_targets;
};


struct Vk_Acceleration_Structure
{
	enum Level
	{
		BOTTOM_LEVEL = 0,
		TOP_LEVEL
	};
	VkAccelerationStructureKHR acceleration_structure;
	Level level;
	Vk_Allocated_Buffer acceleration_structure_buffer;
	VkDeviceAddress acceleration_structure_buffer_address;
	Vk_Allocated_Buffer scratch_buffer;
	VkDeviceAddress scratch_buffer_address;

	// Only used for TLAS
	Vk_Allocated_Buffer tlas_instances;
	VkDeviceAddress tlas_instances_address;
};


struct Scene
{
	std::optional<Vk_Acceleration_Structure> tlas;
	struct Camera_Component* active_camera;
};

struct Texture
{
	Vk_Allocated_Image image;

};

struct Renderer
{
	i32 window_width, window_height;
	Vk_Context* context;
	Platform* platform;
	bool initialized = false;

	Vk_Pipeline rt_pipeline;
	Vk_Framebuffer framebuffer;
	Vk_Allocated_Image final_output; // This is what gets blitted into the swapchain at the end
	uint64_t frame_counter = 0;
	u32 frames_accumulated = 0;
	uint32_t swapchain_image_index = 0;
	VkDescriptorSetLayout desc_set_layout;
	Vk_Allocated_Buffer shader_binding_table;
	Scene scene;
	Vk_Pipeline compute_pp;
	Vk_Allocated_Buffer gpu_camera_data;
	Vk_Allocated_Image environment_map;
	VkSampler bilinear_sampler;

	Resource_Manager<Mesh>* mesh_manager;
	Resource_Manager<Texture>* texture_manager;

	Renderer(Vk_Context* context, Platform* platform, Resource_Manager<Mesh>* mesh_manager, Resource_Manager<Texture>* texture_manager);

	void initialize();

	uint32_t get_frame_index() { return frame_counter % FRAMES_IN_FLIGHT; }
	VkCommandBuffer get_current_frame_command_buffer();
	void vk_create_descriptor_set_layout();
	void vk_create_render_targets(VkCommandBuffer cmd);
	void transition_swapchain_images(VkCommandBuffer cmd);
	void create_samplers();

	void vk_command_buffer_single_submit(VkCommandBuffer cmd);
	Vk_Pipeline vk_create_rt_pipeline();

	void create_vertex_buffer(Mesh* mesh, VkCommandBuffer cmd);
	void create_index_buffer(Mesh* mesh, VkCommandBuffer cmd);
	void create_bottom_level_acceleration_structure(Mesh* mesh);
	void build_bottom_level_acceleration_structure(Mesh* mesh, VkCommandBuffer cmd);
	void create_top_level_acceleration_structure(ECS* ecs, VkCommandBuffer cmd);


	void init_scene(ECS* ecs);
	void pre_frame();
	void begin_frame();
	void end_frame();
	void draw(ECS* ecs);
};

