#pragma once
#include "ecs.h"
#include "renderer.h"

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 texcoord;
};

struct Mesh
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::optional<Vk_Acceleration_Structure> blas;
	Vk_Allocated_Buffer vertex_buffer;
	Vk_Allocated_Buffer index_buffer;
	VkDeviceAddress vertex_buffer_address;
	VkDeviceAddress index_buffer_address;

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

Mesh* get_mesh(ECS* ecs, uint32_t entity_id);