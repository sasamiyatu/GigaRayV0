#define _CRT_SECURE_NO_WARNINGS
#include "gltf.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#include "common.h"
#include "janitor.h"
#include "r_mesh.h"
#include "resource_manager.h"
#include "texture.h"
#include <filesystem>

Mesh2 load_gltf_from_file(const char* filepath, Vk_Context* ctx, Resource_Manager<Texture>* texture_manager, Resource_Manager<Material>* material_manager, bool swap_y_and_z)
{
    Mesh2 ret{};

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, filepath, &data);
    assert(result == cgltf_result_success);

	result = cgltf_load_buffers(&options, data, filepath);
    assert(result == cgltf_result_success);

	std::filesystem::path dir = std::filesystem::path(filepath).parent_path();

    std::map<cgltf_material*, int> local_material_map;
    for (int i = 0; i < data->materials_count; ++i)
    {
        assert(data->materials[i].has_pbr_metallic_roughness == 1);
        //assert(data->materials[i].pbr_metallic_roughness.base_color_texture.texture != nullptr);
        //assert(data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr);
        Material new_mat{};
        {
            if (data->materials[i].pbr_metallic_roughness.base_color_texture.texture != nullptr)
            {
                cgltf_texture_view view = data->materials[i].pbr_metallic_roughness.base_color_texture;
                cgltf_texture* tex = view.texture;
                cgltf_image* img = tex->image;
                std::string file_path = (dir / std::filesystem::path(img->uri)).string();
                i32 id = texture_manager->get_id_from_string(file_path);
                if (id == -1)
                {
                    Vk_Allocated_Image image = ctx->load_texture(file_path.c_str(), false, true);
                    Texture t = { image };
                    id = texture_manager->register_resource(t, file_path);
                }
                new_mat.base_color = id;
            }
            else
            {
                new_mat.base_color = -1;
                
            }
            float* factor = data->materials[i].pbr_metallic_roughness.base_color_factor;
            new_mat.base_color_factor = glm::vec4(factor[0], factor[1], factor[2], factor[3]);
        }

        {
            if (data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr)
            {
                cgltf_texture_view view = data->materials[i].pbr_metallic_roughness.metallic_roughness_texture;
                cgltf_texture* tex = view.texture;
                cgltf_image* img = tex->image;
                std::string file_path = (dir / std::filesystem::path(img->uri)).string();
                i32 id = texture_manager->get_id_from_string(file_path);
                if (id == -1)
                {
                    Vk_Allocated_Image image = ctx->load_texture(file_path.c_str(), false, true);
                    Texture t = { image };
                    id = texture_manager->register_resource(t, file_path);
                }

                new_mat.metallic_roughness = (int)cgltf_texture_index(data, tex);
            }
            else
            {
                new_mat.metallic_roughness = -1;
            }
            new_mat.metallic_factor = data->materials[i].pbr_metallic_roughness.metallic_factor;
            new_mat.roughness_factor = data->materials[i].pbr_metallic_roughness.roughness_factor;
        }
        std::string name = data->materials[i].name != nullptr ? data->materials[i].name : std::to_string(i);
        i32 material_id = material_manager->register_resource(new_mat, name);
        local_material_map[&data->materials[i]] = material_id;
    }

    for (int n = 0; n < data->nodes_count; ++n)
    {
        cgltf_node* node = &data->nodes[n];
        glm::mat4 model;
        cgltf_node_transform_world(node, (float*)&model);
        if (node->mesh)
        {
            cgltf_mesh* mesh = node->mesh;
            for (int j = 0; j < mesh->primitives_count; ++j)
            {
                cgltf_primitive* prim = &mesh->primitives[j];
                // Read indices
                std::vector<u32> indices(prim->indices->count);
                cgltf_size indices_read = cgltf_accessor_unpack_indices(prim->indices, indices.data(), sizeof(uint32_t), indices.size());
                assert(indices_read == indices.size());

                std::vector<glm::vec3> normals;
                std::vector<glm::vec3> positions;
                std::vector<glm::vec2> texcoords;
                std::vector<glm::vec4> tangents;
                u32 attr_count = (u32)mesh->primitives[j].attributes_count;
                for (u32 attr = 0; attr < attr_count; ++attr)
                {
                    cgltf_attribute a = mesh->primitives[j].attributes[attr];
                    cgltf_accessor* acc = a.data;
                    if (a.type == cgltf_attribute_type_normal)
                    {
                        assert(acc->type == cgltf_type_vec3);
                        assert(acc->count != 0);
                        assert(acc->component_type == cgltf_component_type_r_32f);
                        normals.resize(acc->count);
                        cgltf_size num_floats = cgltf_accessor_unpack_floats(acc, nullptr, 0);
						cgltf_accessor_unpack_floats(acc, glm::value_ptr(normals[0]), num_floats);
                    }
                    else if (a.type == cgltf_attribute_type_position)
                    {
                        assert(acc->type == cgltf_type_vec3);
                        assert(acc->count != 0);
                        assert(acc->component_type == cgltf_component_type_r_32f);
                        positions.resize(acc->count);
                        cgltf_size num_floats = cgltf_accessor_unpack_floats(acc, nullptr, 0);
                        cgltf_accessor_unpack_floats(acc, glm::value_ptr(positions[0]), num_floats);
                    }
                    else if (a.type == cgltf_attribute_type_texcoord)
                    {
                        assert(acc->type == cgltf_type_vec2);
                        assert(acc->count != 0);
                        assert(acc->component_type == cgltf_component_type_r_32f);
                        texcoords.resize(acc->count);
                        cgltf_size num_floats = cgltf_accessor_unpack_floats(acc, nullptr, 0);
                        cgltf_accessor_unpack_floats(acc, glm::value_ptr(texcoords[0]), num_floats);
                    }
                    else if (a.type == cgltf_attribute_type_tangent)
                    {
                        assert(acc->type == cgltf_type_vec4);
                        assert(acc->count != 0);
                        assert(acc->component_type == cgltf_component_type_r_32f);
                        tangents.resize(acc->count);
                        cgltf_size num_floats = cgltf_accessor_unpack_floats(acc, nullptr, 0);
                        cgltf_accessor_unpack_floats(acc, glm::value_ptr(tangents[0]), num_floats);
                    }
                    else
                    {
                        assert(false && !"We don't know what the fuck this attribute type is");
                    }
                }

                if (swap_y_and_z)
                {
                    for (auto& pos : positions)
                        pos = glm::vec3(pos.x, pos.z, -pos.y);
                    for (auto& normal : normals)
                        normal = glm::vec3(normal.x, normal.z, -normal.y);
                }

                //assert(!tangents.empty());
                Vertex_Group vg{};
                vg.pos = std::move(positions);
                vg.normal = std::move(normals);
                vg.texcoord = std::move(texcoords);
                vg.indices = std::move(indices);
                vg.tangent = std::move(tangents);
                vg.model = model;

                i32 material_id = local_material_map.at(prim->material);
                //i32 material_id = material_manager->get_id_from_string(prim->material->name);
                ret.materials.push_back(material_id);
                ret.meshes.push_back(vg);
            }
        }
    }

    cgltf_free(data);

    return ret;
}

void create_from_mesh2(Mesh2* m, u32 mesh_count, Mesh* out_meshes)
{
    assert(mesh_count == m->meshes.size());
    for (u32 i = 0; i < mesh_count; ++i)
    {
        Mesh* mesh = &out_meshes[i];
        Mesh_Primitive prim{};
        prim.material_id = m->materials[i];
        prim.vertex_count = (u32)m->meshes[i].indices.size();
        prim.vertex_offset = 0;
        mesh->primitives.push_back(prim);
        mesh->indices = m->meshes[i].indices;
        //assert(m->meshes[i].pos.size() == m->meshes[i].normal.size()
        //    && m->meshes[i].pos.size() == m->meshes[i].texcoord.size());
        u32 vertex_count = (u32)m->meshes[i].pos.size();
        out_meshes[i].vertices.resize(vertex_count);
        for (u32 j = 0; j < vertex_count; ++j)
        {
            Vertex new_vert{};
            glm::vec4 transformed_pos = m->meshes[i].model * glm::vec4(m->meshes[i].pos[j], 1.0f);
            //new_vert.pos = m->meshes[i].pos[j];
            new_vert.pos = glm::vec3(transformed_pos);
            new_vert.normal = m->meshes[i].normal[j];
            if (j < m->meshes[i].texcoord.size())
                new_vert.texcoord = m->meshes[i].texcoord[j];
            if (!m->meshes[i].tangent.empty())
                new_vert.tangent = m->meshes[i].tangent[j];
            else
                new_vert.tangent = glm::vec4(1.f, 0.f, 0.f, 1.f);
            out_meshes[i].vertices[j] = new_vert;
            out_meshes[i].bbmax = glm::max(new_vert.pos, mesh->bbmax);
            out_meshes[i].bbmin = glm::min(new_vert.pos, mesh->bbmin);
        }
    }
}
