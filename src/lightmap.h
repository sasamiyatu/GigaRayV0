#pragma once
#include "r_vulkan.h"
#include "defines.h"
#include "cgltf/cgltf.h"
#include "xatlas.h"
#include "ecs.h"

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

	glm::vec4 base_color_factor;
	float roughness_factor;
	float metallic_factor;
};

struct Attribute
{
	VkBuffer buffer;
	u32 offset;
	u32 count;
};

struct Index_Info
{
	enum Index_Type
	{
		UINT16 = 0,
		UINT32
	};
	Index_Type index_type;
	u32 count;
	VkBuffer buffer;
	u32 offset;
	u32 stride;
};

struct Primitive
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec4> tangents;
	std::vector<glm::vec2> uv0;

	Index_Info indices;

	Attribute position;
	Attribute normal;
	Attribute texcoord0;
	Attribute texcoord1; // lightmap texcoord
	Attribute tangent;

	Material* material;
};


struct Mesh
{
	std::vector<Primitive> primitives;
};

struct Raw_Buffer
{
	u8* data;
	u32 size;
};

struct Render_Target
{
	Vk_Allocated_Image image;
	VkImageLayout layout;
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

struct Lightmap_Renderer
{
	Vk_Context* ctx;
	std::vector<Vk_Allocated_Image> images;
	std::vector<Vk_Allocated_Buffer> buffers;
	std::vector<Raw_Buffer> raw_buffers;

	std::vector<Texture2D> textures;
	std::vector<Material> materials;
	std::vector<Mesh> meshes;

	VkSampler default_sampler = 0;
	VkSampler block_sampler = 0;
	Vk_Pipeline pipeline = {};
	Render_Target color_target;
	Render_Target depth_target;

	u32 window_width = 0;
	u32 window_height = 0;
	float aspect_ratio = 1.0f;

	u64 frame_counter = 0;

	cgltf_data* scene_data;

	xatlas::Atlas* atlas;

	// TEMP
	Vk_Allocated_Buffer vertex_buffer{};
	Vk_Allocated_Buffer index_buffer{};

	Vk_Allocated_Image checkerboard_texture;

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
