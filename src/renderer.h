#pragma once
#include "Volk/volk.h"
#include "vma/vk_mem_alloc.h"
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
#include "sh.h"
#include "texture.h"
#include "scene.h"

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
struct Mesh;

#define NEEDS_TEMPORAL

#ifdef NEEDS_TEMPORAL
constexpr u32 GBUFFER_LAYERS = 2;
#else
constexpr u32 GBUFFER_LAYERS = 1;
#endif

struct Render_Target
{
	Vk_Allocated_Image images[GBUFFER_LAYERS];
	VkFormat format;
	VkImageLayout layout;
	std::string name;
};

enum Render_Targets
{
	PATH_TRACER_COLOR = 0,
	RASTER_COLOR,
	DEPTH,
	NORMAL_ROUGHNESS,
	BASECOLOR_METALNESS,
	WORLD_POSITION,
	INDIRECT_DIFFUSE,
	DENOISER_OUTPUT,
	MAX_RENDER_TARGETS
};

struct Framebuffer
{
	Render_Target render_targets[MAX_RENDER_TARGETS];
};


enum Pipelines
{
	PATH_TRACER_PIPELINE = 0,
	RASTER_PIPELINE,
	CUBEMAP_PIPELINE,
	GENERATE_CUBEMAP_PIPELINE,
	GENERATE_CUBEMAP_PIPELINE2,
	SKYBOX_PIPELINE,
	INDIRECT_DIFFUSE_PIPELINE,
	COMPOSITION_PIPELINE,
	TEMPORAL_ACCUMULATION,
	PIPELINE_COUNT,
};

enum Samplers
{
	POINT_SAMPLER = 0,
	BILINEAR_SAMPLER,
	CUBEMAP_SAMPLER,
	SAMPLER_COUNT
};

#define BLUE_NOISE_TEXTURE_COUNT 64

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

	Vk_Pipeline pipelines[PIPELINE_COUNT];
	VkSampler samplers[SAMPLER_COUNT];

	Raytracing_Pipeline primary_ray_pipeline;

	Framebuffer framebuffer;
	Vk_Allocated_Image final_output; // This is what gets blitted into the swapchain at the end
	uint64_t frame_counter = 0;
	u32 frames_accumulated = 0;
	u32 current_frame_index = 0;
	u32 current_frame_gbuffer_index = 0;
	u32 previous_frame_gbuffer_index = 0;
	uint32_t swapchain_image_index = 0;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout bindless_set_layout;
	VkDescriptorSet bindless_descriptor_set;
	VkDescriptorUpdateTemplate descriptor_update_template;
	Vk_Allocated_Buffer shader_binding_table;
	Scene scene;
	GPU_Buffer gpu_camera_data;
	Vk_Allocated_Image environment_map;
	VkSampler bilinear_sampler;
	VkSampler bilinear_sampler_clamp;
	VkQueryPool query_pools[FRAMES_IN_FLIGHT];
	Vk_Allocated_Image brdf_lut;
	Vk_Allocated_Image prefiltered_envmap;
	Vk_Allocated_Image blue_noise[BLUE_NOISE_TEXTURE_COUNT];
	Cubemap cubemap;
	GPU_Buffer indirect_draw_buffer;
	GPU_Buffer instance_data_buffer;

	Render_Mode render_mode = PATH_TRACER;

	double current_frame_gpu_time;
	double cpu_frame_begin;
	double cpu_frame_end;

	Resource_Manager<Mesh>* mesh_manager;
	Resource_Manager<Texture>* texture_manager;
	Resource_Manager<Material>* material_manager;

	Probe_System probe_system;

	Timer* timer;

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
	void create_render_targets(VkCommandBuffer cmd);
	void transition_swapchain_images(VkCommandBuffer cmd);
	void create_samplers();

	void vk_command_buffer_single_submit(VkCommandBuffer cmd);
	Vk_Pipeline vk_create_rt_pipeline();
	Raytracing_Pipeline create_gbuffer_rt_pipeline();
	Vk_Pipeline create_raster_graphics_pipeline(const char* vertex_shader_path, const char* fragment_shader_path, 
		bool use_bindless_layout, Raster_Options opt = {});

	void create_bottom_level_acceleration_structure(Mesh* mesh);
	void build_bottom_level_acceleration_structure(Mesh* mesh, VkCommandBuffer cmd);
	void create_top_level_acceleration_structure(ECS* ecs, VkCommandBuffer cmd);
	Vk_Allocated_Image prefilter_envmap(VkCommandBuffer cmd, Vk_Allocated_Image envmap);

	void create_lookup_textures();
	void create_cubemap_from_envmap();
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

