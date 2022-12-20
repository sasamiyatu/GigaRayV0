#pragma once
#include "ecs.h"
#include "renderer.h"

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 texcoord;
	glm::vec4 tangent;
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

Mesh* get_mesh(ECS* ecs, uint32_t entity_id);

Mesh create_sphere(u32 subdivision);