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
#include "material.h"
#include "gbuffer.h"
#include "timer.h"

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


struct Render_Target
{
	Vk_Allocated_Image image;
	VkFormat format;
	VkImageLayout layout;
	std::string name;
};

struct Framebuffer
{
	std::vector<Render_Target> render_targets;
};


struct Acceleration_Structure
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

struct GPU_Camera_Data
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::uvec4 frame_index;
};

struct Scene
{
	std::optional<Acceleration_Structure> tlas;
	struct Camera_Component* active_camera;
	GPU_Camera_Data current_frame_camera;
	Vk_Allocated_Buffer material_buffer;
};

struct Texture
{
	Vk_Allocated_Image image;

};

struct Gbuffer
{
	Render_Target normal;
	Render_Target world_pos;
	Render_Target albedo;
	Render_Target depth;
};

enum Render_Targets
{
	PATH_TRACER_COLOR = 0,
	RASTER_COLOR,
	DEPTH,
	MAX
};

struct Renderer
{
	enum Render_Mode {
		PATH_TRACER = 0,
		RASTER,
		SIDE_BY_SIDE
	};
	
	i32 window_width, window_height;
	float aspect_ratio;
	Vk_Context* context;
	Platform* platform;
	bool initialized = false;

	Vk_Pipeline rt_pipeline;
	Raytracing_Pipeline primary_ray_pipeline;
	Vk_Pipeline raster_pipeline;

	Framebuffer framebuffer;
	Vk_Allocated_Image final_output; // This is what gets blitted into the swapchain at the end
	uint64_t frame_counter = 0;
	u32 frames_accumulated = 0;
	u32 current_frame_index = 0;
	uint32_t swapchain_image_index = 0;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout bindless_set_layout;
	VkDescriptorSet bindless_descriptor_set;
	VkDescriptorUpdateTemplate descriptor_update_template;
	Vk_Allocated_Buffer shader_binding_table;
	Scene scene;
	Vk_Pipeline compute_pp;
	GPU_Buffer gpu_camera_data;
	Vk_Allocated_Image environment_map;
	VkSampler bilinear_sampler;
	VkSampler bilinear_sampler_clamp;
	VkQueryPool query_pools[FRAMES_IN_FLIGHT];
	Vk_Allocated_Image brdf_lut;
	Vk_Allocated_Image prefiltered_envmap;
	GPU_Buffer indirect_draw_buffer;

	Render_Mode render_mode = PATH_TRACER;

	double current_frame_gpu_time;
	double cpu_frame_begin;
	double cpu_frame_end;

	Resource_Manager<Mesh>* mesh_manager;
	Resource_Manager<Texture>* texture_manager;
	Resource_Manager<Material>* material_manager;

	Timer* timer;

	Gbuffer gbuffer;

	Renderer(Vk_Context* context, Platform* platform, 
		Resource_Manager<Mesh>* mesh_manager, 
		Resource_Manager<Texture>* texture_manager,
		Resource_Manager<Material>* material_manager,
		Timer* timer);

	void initialize();

	VkCommandBuffer get_current_frame_command_buffer();
	void create_descriptor_pools();
	void create_bindless_descriptor_set_layout();
	VkDescriptorSetLayout create_descriptor_set_layout(struct Shader* shader);
	void vk_create_render_targets(VkCommandBuffer cmd);
	void transition_swapchain_images(VkCommandBuffer cmd);
	void create_samplers();

	void vk_command_buffer_single_submit(VkCommandBuffer cmd);
	Vk_Pipeline vk_create_rt_pipeline();
	Raytracing_Pipeline create_gbuffer_rt_pipeline();
	Vk_Pipeline create_raster_pipeline();

	void create_vertex_buffer(Mesh* mesh, VkCommandBuffer cmd);
	void create_index_buffer(Mesh* mesh, VkCommandBuffer cmd);
	void create_bottom_level_acceleration_structure(Mesh* mesh);
	void build_bottom_level_acceleration_structure(Mesh* mesh, VkCommandBuffer cmd);
	void create_top_level_acceleration_structure(ECS* ecs, VkCommandBuffer cmd);
	Vk_Allocated_Image prefilter_envmap(VkCommandBuffer cmd, Vk_Allocated_Image envmap);

	void create_lookup_textures();

	void do_frame(ECS* ecs);
	void init_scene(ECS* ecs);
	void pre_frame();
	void begin_frame();
	void render_gbuffer();
	void trace_primary_rays();
	void end_frame();
	void draw(ECS* ecs);
	void trace_rays(VkCommandBuffer cmd);
	void tonemap(VkCommandBuffer cmd);
	void rasterize(VkCommandBuffer cmd, ECS* ecs);
	void cleanup();
};

