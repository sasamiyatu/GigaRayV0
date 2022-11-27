#pragma once

#include "renderer.h"
//#include "resource_manager.h"

struct Material
{
	i32 base_color = -1;
	i32 metallic_roughness = -1;
	i32 normal_map = -1;
};

struct Vertex_Group
{
	std::vector<glm::vec3> pos;
	std::vector<glm::vec3> normal;
	std::vector<glm::vec2> texcoord;
	std::vector<u32> indices;
};

struct Mesh2
{
	std::vector<Vertex_Group> meshes;
	std::vector<Material> materials;
};

Mesh2 load_gltf_from_file(const char* filepath, Vk_Context* ctx, Resource_Manager<Texture>* texture_manager);

void create_from_mesh2(Mesh2* m, u32 mesh_count, Mesh* out_meshes);