#pragma once
#include "defines.h"
#include "material.h"
//#include "renderer.h"
//#include "resource_manager.h"
struct Mesh;
struct Vk_Context;
struct Texture;

template<typename T>
struct Resource_Manager;


struct Vertex_Group
{
	std::vector<glm::vec3> pos;
	std::vector<glm::vec3> normal;
	std::vector<glm::vec2> texcoord;
	std::vector<glm::vec4> tangent;
	std::vector<u32> indices;
	glm::mat4 model;
};

struct Mesh2
{
	std::vector<Vertex_Group> meshes;
	std::vector<i32> materials; 
};

Mesh2 load_gltf_from_file(const char* filepath, Vk_Context* ctx, Resource_Manager<Texture>* texture_manager, Resource_Manager<Material>* material_manager, bool swap_y_and_z = false);

void create_from_mesh2(Mesh2* m, u32 mesh_count, Mesh* out_meshes);