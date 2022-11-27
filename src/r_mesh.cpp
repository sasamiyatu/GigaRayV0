#include "r_mesh.h"
#include "resource_manager.h"

Mesh* get_mesh(ECS* ecs, uint32_t entity_id)
{
	Static_Mesh_Component* mesh = ecs->get_component<Static_Mesh_Component>(entity_id);
	if (!mesh) return nullptr;

	Mesh* m = mesh->manager->get_resource_with_id(mesh->mesh_id);
	if (!m) return nullptr;

	return m;
}

uint32_t Mesh::get_vertex_buffer_size()
{
	return (uint32_t)(sizeof(vertices[0]) * vertices.size());
}

uint32_t Mesh::get_index_buffer_size()
{
	return (uint32_t)(sizeof(uint32_t) * indices.size());
}

void Mesh::get_acceleration_structure_build_info(
	VkAccelerationStructureBuildGeometryInfoKHR* build_info,
	VkAccelerationStructureGeometryKHR* geometry,
	VkAccelerationStructureBuildRangeInfoKHR* range_info)
{
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vertex_buffer_address;
	triangles.vertexStride = get_vertex_size();
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = index_buffer_address;
	triangles.maxVertex = get_vertex_count() - 1;
	triangles.transformData = { 0 };

	*geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry->geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry->geometry.triangles = triangles;
	geometry->flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	*range_info = {};
	range_info->firstVertex = 0;
	range_info->primitiveCount = get_primitive_count();
	range_info->primitiveOffset = 0;
	range_info->transformOffset = 0;

	*build_info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	build_info->flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	build_info->geometryCount = 1;
	build_info->pGeometries = geometry;
	build_info->mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	build_info->type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	build_info->srcAccelerationStructure = VK_NULL_HANDLE;
}

uint32_t Mesh::get_vertex_count()
{
	return (uint32_t)vertices.size();
}

uint32_t Mesh::get_vertex_size()
{
	return (uint32_t)(sizeof(vertices[0]));
}

uint32_t Mesh::get_primitive_count()
{
	return (uint32_t)(indices.size() / 3);
}
