#include "r_mesh.h"
#include "ecs.h"
#include "resource_manager.h"

void merge_meshes(u32 num_meshes, Mesh* meshes, Mesh* out)
{
	assert(out->vertices.empty());
	assert(out->indices.empty());
	for (u32 i = 0; i < num_meshes; ++i)
	{
		u32 start_index = (u32)out->vertices.size();
		u32 index_offset = (u32)out->indices.size();
		u32 index_count = (u32)meshes[i].indices.size();
		out->vertices.insert(out->vertices.end(), meshes[i].vertices.begin(), meshes[i].vertices.end());
		for (u32 j = 0; j < index_count; ++j)
			out->indices.push_back(start_index + meshes[i].indices[j]);
		Mesh_Primitive prim{};
		prim.vertex_offset = index_offset;
		prim.vertex_count = index_count;
		assert(meshes[i].primitives.size() == 1);
		prim.material_id = meshes[i].primitives[0].material_id;
		out->primitives.push_back(prim);
		out->bbmax = glm::max(out->bbmax, meshes[i].bbmax);
		out->bbmin = glm::min(out->bbmin, meshes[i].bbmin);
	}
}

Mesh* get_mesh(ECS* ecs, uint32_t entity_id)
{
	Static_Mesh_Component* mesh = ecs->get_component<Static_Mesh_Component>(entity_id);
	if (!mesh) return nullptr;

	Mesh* m = mesh->manager->get_resource_with_id(mesh->mesh_id);
	if (!m) return nullptr;

	return m;
}

Mesh create_sphere(u32 subdivision)
{
	Mesh mesh;

	float step_size = 2.f / subdivision;
	u32 w = subdivision + 1;
	u32 verts_per_face = w * w;
	u32 start_face = 0;
	u32 end_face = 6;
	for (u32 face = start_face; face < end_face; ++face)
	{
		for (u32 i = 0; i <= subdivision; ++i)
		{
			for (u32 j = 0; j <= subdivision; ++j)
			{
				float x = -1.f + step_size * j;
				float y = -1.f + step_size * i;
				float z = 1.f;
				Vertex v{};
				glm::vec3 pos;
				switch (face)
				{
				case 0:
					pos = normalize(glm::vec3(-x, y, z));
					break;
				case 1:
					pos = normalize(glm::vec3(x, y, -z));
					break;
				case 2:
					pos = normalize(glm::vec3(z, -x, y));
					break;
				case 3:
					pos = normalize(glm::vec3(-z, x, y));
					break;
				case 4:
					pos = normalize(glm::vec3(x, z, y));
					break;
				case 5:
					pos = normalize(glm::vec3(-x, -z, y));
					break;
				default:
					assert(false);
					break;
				}
				v.pos = pos * 10.f;
				float theta = acosf(-pos.y);
				float phi = atan2f(-pos.z, pos.x) + (float)M_PI;
				glm::vec2 uv = glm::vec2(phi / (2.0f * M_PI), theta / M_PI);
				v.texcoord = uv;
				v.normal = pos;
				mesh.vertices.push_back(v);
			}
		}
	}

	u32 start_index = 0;
	for (u32 face = start_face; face < end_face; ++face)
	{
		for (u32 i = 0; i < subdivision; ++i)
		{
			for (u32 j = 0; j < subdivision; ++j)
			{
				glm::ivec3 first_triangle = glm::ivec3(j * w + i, (j + 1) * w + i, (j + 1) * w + i + 1) + glm::ivec3(start_index);
				glm::ivec3 second_triangle = glm::ivec3(j * w + i, (j + 1) * w + i + 1, j * w + i + 1) + glm::ivec3(start_index);
				for (int ii = 0; ii < 3; ++ii)
					mesh.indices.push_back(first_triangle[ii]);
				for (int ii = 0; ii < 3; ++ii)
					mesh.indices.push_back(second_triangle[ii]);
			}
		}
		start_index += verts_per_face;
	}

	Mesh_Primitive prim = {};
	prim.material_id = 0;
	prim.vertex_count = (u32)mesh.indices.size();
	prim.vertex_offset = 0;
	mesh.primitives.push_back(prim);
	mesh.bbmin = glm::vec3(-1.0);
	mesh.bbmax = glm::vec3(1.0);

	return mesh;
}

Mesh create_box(float x_scale, float y_scale, float z_scale)
{
	Mesh mesh;

	for (u32 face = 0; face < 6; ++face)
	{
		for (int i = 0; i < 2; ++i)
		{
			for (int j = 0; j < 2; ++j)
			{
				float x = -1.f + 2.0f * j;
				float y = -1.f + 2.0f * i;
				float z = 1.f;
				Vertex v{};
				glm::vec3 pos;
				glm::vec3 normal;
				switch (face)
				{
				case 0:
					pos = normalize(glm::vec3(-x, y, z));
					normal = glm::vec3(0.0f, 0.0f, 1.0f);
					break;
				case 1:
					pos = normalize(glm::vec3(x, y, -z));
					normal = glm::vec3(0.0f, 0.0f, -1.0f);

					break;
				case 2:
					pos = normalize(glm::vec3(z, -x, y));
					normal = glm::vec3(1.0f, 0.0f, 0.0f);

					break;
				case 3:
					pos = normalize(glm::vec3(-z, x, y));
					normal = glm::vec3(-1.0f, 0.0f, 0.0f);

					break;
				case 4:
					pos = normalize(glm::vec3(x, z, y));
					normal = glm::vec3(0.0f, 1.0f, 0.0f);

					break;
				case 5:
					pos = normalize(glm::vec3(-x, -z, y));
					normal = glm::vec3(0.0f, -1.0f, 0.0f);

					break;
				default:
					assert(false);
					break;
				}
				v.pos = pos;
				v.texcoord = glm::vec2(x * 0.5f + 0.5f, y * 0.5f + 0.5f);
				v.normal = normal;
				mesh.vertices.push_back(v);
			}
		}
	}

	u32 start_index = 0;
	for (u32 face = 0; face < 6; ++face)
	{
		glm::ivec3 first_triangle = glm::ivec3(0, 2, 3) + glm::ivec3(start_index);
		glm::ivec3 second_triangle = glm::ivec3(0, 3, 1) + glm::ivec3(start_index);
		for (int ii = 0; ii < 3; ++ii)
			mesh.indices.push_back(first_triangle[ii]);
		for (int ii = 0; ii < 3; ++ii)
			mesh.indices.push_back(second_triangle[ii]);
		start_index += 4;
	}

	Mesh_Primitive prim{};
	prim.vertex_count = (u32)mesh.indices.size();
	prim.vertex_offset = 0;
	mesh.primitives.push_back(prim);

	return mesh;
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

void create_vertex_buffer(Mesh* mesh, VkCommandBuffer cmd, Vk_Context* ctx)
{
	u32 buffer_size = mesh->get_vertex_buffer_size();
	mesh->vertex_buffer = ctx->allocate_buffer(buffer_size,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
		| VK_BUFFER_USAGE_TRANSFER_DST_BIT
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);


	mesh->vertex_buffer_address = ctx->get_buffer_device_address(mesh->vertex_buffer);

	Vk_Allocated_Buffer tmp_staging_buffer = ctx->allocate_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 0);


	void* mapped;
	vmaMapMemory(ctx->allocator, tmp_staging_buffer.allocation, &mapped);
	memcpy(mapped, mesh->vertices.data(), buffer_size);
	vmaUnmapMemory(ctx->allocator, tmp_staging_buffer.allocation);

	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0; // Optional
	copy_region.dstOffset = 0; // Optional
	copy_region.size = buffer_size;

	vkCmdCopyBuffer(cmd, tmp_staging_buffer.buffer, mesh->vertex_buffer.buffer, 1, &copy_region);
}

void create_index_buffer(Mesh* mesh, VkCommandBuffer cmd, Vk_Context* ctx)
{
	u32 buffer_size = mesh->get_index_buffer_size();
	mesh->index_buffer = ctx->allocate_buffer(buffer_size,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_INDEX_BUFFER_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
		| VK_BUFFER_USAGE_TRANSFER_DST_BIT
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);


	mesh->index_buffer_address = ctx->get_buffer_device_address(mesh->index_buffer);

	Vk_Allocated_Buffer tmp_staging_buffer = ctx->allocate_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 0);


	void* mapped;
	vmaMapMemory(ctx->allocator, tmp_staging_buffer.allocation, &mapped);
	memcpy(mapped, mesh->indices.data(), buffer_size);
	vmaUnmapMemory(ctx->allocator, tmp_staging_buffer.allocation);

	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0; // Optional
	copy_region.dstOffset = 0; // Optional
	copy_region.size = buffer_size;

	vkCmdCopyBuffer(cmd, tmp_staging_buffer.buffer, mesh->index_buffer.buffer, 1, &copy_region);
}