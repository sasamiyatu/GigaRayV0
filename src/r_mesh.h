#pragma once
#include "defines.h"
#include "r_vulkan.h"

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 texcoord;
	glm::vec4 tangent;
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

struct Mesh_Primitive
{
	u32 vertex_offset;
	u32 vertex_count;
	u32 material_id;
	std::optional<Acceleration_Structure> acceleration_structure;
};

struct Mesh
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Mesh_Primitive> primitives;
	glm::vec3 bbmin = glm::vec3(INFINITY);
	glm::vec3 bbmax = glm::vec3(-INFINITY);
	
	Vk_Allocated_Buffer vertex_buffer;
	Vk_Allocated_Buffer index_buffer;
	VkDeviceAddress vertex_buffer_address;
	VkDeviceAddress index_buffer_address;
	
	GPU_Buffer instance_data_buffer;

	uint32_t get_vertex_buffer_size();
	uint32_t get_index_buffer_size();
	uint32_t get_vertex_count();
	uint32_t get_vertex_size();
	uint32_t get_primitive_count();

	void get_acceleration_structure_build_info(
		VkAccelerationStructureBuildGeometryInfoKHR* build_info,
		VkAccelerationStructureGeometryKHR* geometry,
		VkAccelerationStructureBuildRangeInfoKHR* range_info);
};

struct Primitive_Info
{
	u32 material_index;
	u32 vertex_count;
	u32 vertex_offset;
};

struct Geometry
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Mesh_Primitive> meshes;
};

void merge_meshes(u32 num_meshes, Mesh* meshes, Mesh* out);

struct ECS;

Mesh* get_mesh(ECS* ecs, uint32_t entity_id);

Mesh create_sphere(u32 subdivision);
Mesh create_box(float x_scale = 1.0f, float y_scale = 1.0f, float z_scale = 1.0f);

void create_vertex_buffer(Mesh* mesh, VkCommandBuffer cmd, Vk_Context* ctx);
void create_index_buffer(Mesh* mesh, VkCommandBuffer cmd, Vk_Context* ctx);