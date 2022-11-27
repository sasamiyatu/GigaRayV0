#define _CRT_SECURE_NO_WARNINGS
#include "gltf.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#include "common.h"

template <typename T>
static void read_data(cgltf_buffer_view* view, u32 stride, const std::map<std::string, u8*>& buffer_data, T* out_data)
{
    assert(stride != 0);
    u8* src_data = buffer_data.at(view->buffer->uri);
    u8* start = src_data + view->offset;
    u8* end = start + view->size;
    assert(stride == sizeof(T));
    memcpy(out_data, start, view->size);
}

Mesh2 load_gltf_from_file(const char* filepath, Vk_Context* ctx, Resource_Manager<Texture>* texture_manager)
{
    Mesh2 ret{};

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, filepath, &data);
    assert(result == cgltf_result_success);

    u32 str_length = (u32)strlen(filepath);
    char* stripped = (char*)malloc(str_length + 1);
    strcpy(stripped, filepath);
    char* slashpos;
    for (slashpos = stripped + str_length - 1; slashpos && *slashpos != '/'; --slashpos)
        ;
    *(slashpos + 1) = 0;

    
    std::map<std::string, u8*> buffers;
    for (int i = 0; i < data->buffers_count; ++i)
    {
        u32 size = (u32)data->buffers[i].size;
        std::string filepath = std::string(stripped) + std::string(data->buffers[i].uri);
        buffers[data->buffers[i].uri] = nullptr;
        u32 bytes_read = read_entire_file(filepath.c_str(), &buffers[data->buffers[i].uri]);
        assert(size == bytes_read);
    }

    std::vector<Material> mats(data->materials_count);
    for (int i = 0; i < data->materials_count; ++i)
    {
        assert(data->materials[i].has_pbr_metallic_roughness == 1);
        assert(data->materials[i].pbr_metallic_roughness.base_color_texture.texture != nullptr);
        assert(data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr);

        {
            cgltf_texture_view view = data->materials[i].pbr_metallic_roughness.base_color_texture;
            cgltf_texture* tex = view.texture;
            cgltf_image* img = tex->image;
            std::string file_path = std::string(stripped) + std::string(img->uri);
            Vk_Allocated_Image image = ctx->load_texture(file_path.c_str());
            g_garbage_collector->push([=]()
                {
                    vmaDestroyImage(ctx->allocator, image.image, image.allocation);
                }, Garbage_Collector::SHUTDOWN);
            Texture t = { image };
            int id = texture_manager->register_resource(t, file_path);
            mats[i].base_color = id;
        }

        {
            cgltf_texture_view view = data->materials[i].pbr_metallic_roughness.metallic_roughness_texture;
            cgltf_texture* tex = view.texture;
            cgltf_image* img = tex->image;
            std::string file_path = std::string(stripped) + std::string(img->uri);
            Vk_Allocated_Image image = ctx->load_texture(file_path.c_str());
            g_garbage_collector->push([=]()
                {
                    vmaDestroyImage(ctx->allocator, image.image, image.allocation);
                }, Garbage_Collector::SHUTDOWN);
            Texture t = { image };
            int id = texture_manager->register_resource(t, file_path);
            mats[i].metallic_roughness = id;
        }

    }

    ret.materials = std::move(mats);

    free(stripped);


    for (int i = 0; i < data->meshes_count; ++i)
    {
        for (int j = 0; j < data->meshes[i].primitives_count; ++j)
        {
            cgltf_primitive* prim = &data->meshes[i].primitives[j];
            
            // Read indices
            std::vector<u32> indices(prim->indices->count);
            cgltf_buffer_view* idx_buf_view = prim->indices->buffer_view;
            u32 stride = (u32)prim->indices->stride;
            cgltf_buffer* buffer = idx_buf_view->buffer;
            u8* buf_data = buffers[buffer->uri];
            u8* start = buf_data + idx_buf_view->offset;
            u8* end = start + idx_buf_view->size;
            assert(stride != 1);

            int index = 0;
            for (u8* ptr = start; ptr != end; ptr += stride, index++)
            {
                if (stride == 2)
                {
                    u16 idx = *(u16*)ptr;
                    indices[index] = (u32)idx;
                }
                else if (stride == 4)
                {
                    indices[index] = *(u32*)ptr;
                }
                else
                {
                    assert(false);
                }
            }

            std::vector<glm::vec3> normals;
            std::vector<glm::vec3> positions;
            std::vector<glm::vec2> texcoords;
            u32 attr_count = (u32)data->meshes[i].primitives[j].attributes_count;
            for (u32 attr = 0; attr < attr_count; ++attr)
            {
                cgltf_attribute a = data->meshes[i].primitives[j].attributes[attr];
                cgltf_accessor* acc = a.data;
                if (a.type == cgltf_attribute_type_normal)
                {
                    assert(acc->type == cgltf_type_vec3);
                    assert(acc->count != 0);
                    assert(acc->component_type == cgltf_component_type_r_32f);
                    normals.resize(acc->count);
                    read_data(acc->buffer_view, (u32)acc->stride, buffers, normals.data());
                }
                else if (a.type == cgltf_attribute_type_position)
                {
                    assert(acc->type == cgltf_type_vec3);
                    assert(acc->count != 0);
                    assert(acc->component_type == cgltf_component_type_r_32f);
                    positions.resize(acc->count);
                    read_data(acc->buffer_view, (u32)acc->stride, buffers, positions.data());
                }
                else if (a.type == cgltf_attribute_type_texcoord)
                {
                    assert(acc->type == cgltf_type_vec2);
                    assert(acc->count != 0);
                    assert(acc->component_type == cgltf_component_type_r_32f);
                    texcoords.resize(acc->count);
                    read_data(acc->buffer_view, (u32)acc->stride, buffers, texcoords.data());
                }
            }

            Vertex_Group vg{};
            vg.pos = std::move(positions);
            vg.normal = std::move(normals);
            vg.texcoord = std::move(texcoords);
            vg.indices = std::move(indices);

            ret.meshes.emplace_back(vg);
        }
    }

    for (auto& pair : buffers)
    {
        free(pair.second);
    }


    return ret;
}

void create_from_mesh2(Mesh2* m, u32 mesh_count, Mesh* out_meshes)
{
    assert(mesh_count == m->meshes.size());
    for (u32 i = 0; i < mesh_count; ++i)
    {
        Mesh* mesh = &out_meshes[i];
        mesh->indices = m->meshes[i].indices;
        assert(m->meshes[i].pos.size() == m->meshes[i].normal.size()
            && m->meshes[i].pos.size() == m->meshes[i].texcoord.size());
        u32 vertex_count = (u32)m->meshes[i].pos.size();
        out_meshes[i].vertices.resize(vertex_count);
        for (u32 j = 0; j < vertex_count; ++j)
        {
            Vertex new_vert{};
            new_vert.pos = m->meshes[i].pos[j];
            new_vert.normal = m->meshes[i].normal[j];
            new_vert.texcoord = m->meshes[i].texcoord[j];
            out_meshes[i].vertices[j] = new_vert;
        }
    }
}
