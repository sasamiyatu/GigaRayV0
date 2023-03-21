#pragma once
#include "r_vulkan.h"
#include "defines.h"
#include "cgltf/cgltf.h"
#include "xatlas.h"
#include "ecs.h"
#include "events.h"

namespace lm 
{

struct Texture2D
{
	Vk_Allocated_Image image;
	VkSampler sampler;
};

struct Material
{
	Texture2D* base_color_tex;
	Texture2D* metallic_roughness_tex;
	Texture2D* normal_map_tex;
	Texture2D* emissive_tex;

	glm::vec4 base_color_factor;
	float roughness_factor;
	float metallic_factor;

	glm::vec3 emissive_factor;
};

struct GPU_Material
{
	int base_color_tex = -1;
	int metallic_roughness_tex = -1;
	int normal_map_tex = -1;
	int emissive_tex = -1;

	glm::vec4 base_color_factor;
	float roughness_factor;
	float metallic_factor;

	glm::vec3 emissive_factor;
};

struct Primitive
{
	Vk_Allocated_Buffer vertex_buffer;
	Vk_Allocated_Buffer index_buffer;
	u32 vertex_count;
	u32 index_count;

	VkAccelerationStructureKHR blas = 0;

	Material* material;
};


struct Mesh
{
	std::vector<Primitive> primitives;

	glm::mat4 xform;
};

struct Render_Target
{
	Vk_Allocated_Image image;
	VkImageLayout layout;
	VkExtent3D extent;
};

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec4 tangent;
	glm::vec2 uv0;
	glm::vec2 uv1;
};

struct Texel_Sample
{
	u32 mesh_index;
	u32 primitive_index;
	glm::uvec2 texel;
	glm::vec3 barycentrics;
};

#define MAX_TEXEL_SAMPLES 8

struct Texel_Sample_Data
{
	u32 sample_count;
	Texel_Sample samples[MAX_TEXEL_SAMPLES];
};

struct GPU_Camera_Data
{
	glm::mat4 viewproj;
	glm::mat4 view;
	glm::mat4 inverse_view;
	glm::mat4 proj;
};

struct Lightmap_Renderer
{
	Vk_Context* ctx;
	std::vector<Vk_Allocated_Image> images;
	std::vector<Vk_Allocated_Buffer> buffers;

	std::vector<Texture2D> textures;
	std::vector<Material> materials;
	std::vector<Mesh> meshes;

	std::vector<Texel_Sample_Data> lm_texel_samples;

	VkAccelerationStructureKHR tlas = 0;

	VkSampler default_sampler = 0;
	VkSampler block_sampler = 0;
	Vk_Pipeline pipeline = {};
	Vk_Pipeline lightmap_gbuffer_pipeline = {};
	Raytracing_Pipeline lightmap_rt_vis_pipeline = {};
	Raytracing_Pipeline lightmap_trace_pipeline = {};
	VkDescriptorSetLayout bindless_set_layout = 0;
	VkDescriptorSet bindless_descriptor_set = 0;
	VkDescriptorPool descriptor_pool;
	Render_Target color_target;
	Render_Target depth_target;

	// These are for lightmaps
	Render_Target lightmap_target;
	Render_Target normal_target;
	Render_Target position_target;

	GPU_Buffer camera_data;
	Vk_Allocated_Buffer mesh_transform_data;
	Vk_Allocated_Buffer lightmap_sample_data;
	Vk_Allocated_Buffer material_data;

	u32 window_width = 0;
	u32 window_height = 0;
	float aspect_ratio = 1.0f;

	u64 frame_counter = 0;
	u64 lightmap_frames_accumulated = 0;

	cgltf_data* scene_data;

	xatlas::Atlas* atlas;

	Vk_Allocated_Image lightmap_texture;

	Camera_Component* camera = nullptr;

	Lightmap_Renderer(Vk_Context* context, u32 window_width, u32 window_height);
	~Lightmap_Renderer();

	void init_scene(const char* gltf_path);
	void set_camera(Camera_Component* camera);
	void render();
	void shutdown();
};

template <typename T>
T min3(T v0, T v1, T v2)
{
	return std::min<T>(std::min<T>(v0, v1), v2);
}

template <typename T>
T max3(T v0, T v1, T v2)
{
	return std::max<T>(std::max<T>(v0, v1), v2);
}

} // namespace lm
