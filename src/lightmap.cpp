#include "lightmap.h"
#include "common.h"
#include "cgltf/cgltf.h"
#include "stb/stb_image.h"
#include "vk_helpers.h"
#include "g_math.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "logging.h"
#include "shaders.h"

constexpr u32 MAX_BINDLESS_RESOURCES = 16384;

namespace lm
{

// Debug use
struct Image
{
    u32 w;
    u32 h;
    u8* data;
};

static void load_textures(Vk_Context* ctx, const char* basepath, cgltf_image* images, u32 image_count, std::vector<Vk_Allocated_Image>& out_images);
static xatlas::MeshDecl create_mesh_decl(cgltf_data* data, cgltf_primitive* prim);
static void render_debug_atlas(xatlas::Atlas* atlas, const char* filename);
static void fill_triangle(Image* img, glm::vec2 uvs[3], glm::vec3 color);
static int orient2d(glm::vec2 a, glm::vec2 b, glm::vec2 c);
static void draw_triangle(Image* img, glm::vec2 uvs[3], glm::vec3 color);
static void draw_line(int x0, int y0, int x1, int y1, Image* img, glm::vec3 color);
static void set_pixel(Image* img, int x, int y, glm::vec3 color);
static void write_image(Image* img, const char* filename);

Lightmap_Renderer::Lightmap_Renderer(Vk_Context* context, u32 window_width, u32 window_height)
    : ctx(context), 
    atlas(nullptr),
    scene_data(nullptr),
    window_width(window_width),
    window_height(window_height),
    aspect_ratio(float(window_width) / float(window_height))
{
}

Lightmap_Renderer::~Lightmap_Renderer()
{
}

template <typename T>
static size_t get_index(T* start, T* offset)
{
    assert(offset >= start);
    return (offset - start);
}

void Lightmap_Renderer::init_scene(const char* gltf_path)
{
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, gltf_path, &data);
    assert(result == cgltf_result_success);
    result = cgltf_load_buffers(&options, data, gltf_path);
    assert(result == cgltf_result_success);
    scene_data = data;

    u32 str_length = (u32)strlen(gltf_path);
    char* stripped = (char*)malloc(str_length + 1);
    strcpy(stripped, gltf_path);
    char* slashpos;
    for (slashpos = stripped + str_length - 1; slashpos && *slashpos != '/'; --slashpos)
        ;
    *(slashpos + 1) = 0;

    load_textures(ctx, stripped, data->images, (u32)data->images_count, images);

    // Create textures
    VkSamplerCreateInfo sampler_info = vkinit::sampler_create_info();
    vkCreateSampler(ctx->device, &sampler_info, nullptr, &default_sampler);
    sampler_info.minFilter = sampler_info.magFilter = VK_FILTER_NEAREST;
    vkCreateSampler(ctx->device, &sampler_info, nullptr, &block_sampler);
    for (u32 i = 0; i < data->textures_count; ++i)
    {
        Texture2D tex{};
        tex.image = images[get_index(data->images, data->textures[i].image)];
        tex.sampler = default_sampler;
        textures.push_back(tex);
    }

    // Create materials
    for (u32 i = 0; i < data->materials_count; ++i)
    {
        Material new_mat{};
        cgltf_material* mat = &data->materials[i];
        assert(mat->has_pbr_metallic_roughness);
        new_mat.base_color_factor = glm::make_vec4(mat->pbr_metallic_roughness.base_color_factor);
        new_mat.roughness_factor = mat->pbr_metallic_roughness.roughness_factor;
        new_mat.metallic_factor = mat->pbr_metallic_roughness.metallic_factor;

        new_mat.base_color_tex = mat->pbr_metallic_roughness.base_color_texture.texture ? &textures[get_index(data->textures, mat->pbr_metallic_roughness.base_color_texture.texture)] : nullptr;
        new_mat.metallic_roughness_tex = mat->pbr_metallic_roughness.metallic_roughness_texture.texture ? &textures[get_index(data->textures, mat->pbr_metallic_roughness.metallic_roughness_texture.texture)] : nullptr;
        new_mat.normal_map_tex = mat->normal_texture.texture ? &textures[get_index(data->textures, mat->normal_texture.texture)] : nullptr;

        materials.push_back(new_mat);
    }

    {
        // Generate atlas
        atlas = xatlas::Create();
        for (u32 m = 0; m < data->meshes_count; ++m)
        {
            for (u32 p = 0; p < data->meshes[m].primitives_count; ++p)
            {
                xatlas::MeshDecl decl = create_mesh_decl(data, &data->meshes[m].primitives[p]);
                xatlas::AddMeshError err = xatlas::AddMesh(atlas, decl);
                assert(err == xatlas::AddMeshError::Success);
            }
        }

        xatlas::ChartOptions chart_opts{};
        xatlas::PackOptions pack_opts{};
        pack_opts.texelsPerUnit = 4.0;
        xatlas::Generate(atlas, chart_opts, pack_opts);
        render_debug_atlas(atlas, "test_atlas.png");

        // Generate UVs and create vertex / index buffers
        u32 atlas_mesh_index = 0;

        VkCommandBuffer cmd = ctx->allocate_command_buffer();
        VkCommandBufferBeginInfo cmd_info = vkinit::command_buffer_begin_info();
        vkBeginCommandBuffer(cmd, &cmd_info);
        for (u32 m = 0; m < data->meshes_count; ++m)
        {
            size_t mesh_index = meshes.size();
            meshes.push_back({});
            for (u32 p = 0; p < data->meshes[m].primitives_count; ++p)
            {
                const xatlas::Mesh* mesh = &atlas->meshes[atlas_mesh_index++];
                const cgltf_primitive* prim = &data->meshes[m].primitives[p];

                Buffer<Vertex> verts(mesh->vertexCount);
                Buffer<u32> indices(mesh->indexCount);

                for (u32 i = 0; i < mesh->vertexCount; ++i)
                {
                    const xatlas::Vertex* vertex = &mesh->vertexArray[i];
                    u32 xref = vertex->xref;
                    u32 chart_index = vertex->chartIndex;
                    glm::vec2 uv = glm::make_vec2(vertex->uv);

                    Vertex v{};

                    for (u32 a = 0; a < prim->attributes_count; ++a)
                    {
                        const cgltf_attribute* attrib = &prim->attributes[a];
                        cgltf_accessor* acc = attrib->data;
                        size_t component_count = cgltf_num_components(acc->type);
                        float* in_ptr = (float*)((u8*)acc->buffer_view->buffer->data + acc->buffer_view->offset + acc->offset + acc->stride * xref);
                        switch (attrib->type)
                        {
                        case cgltf_attribute_type_position:
                            v.position = glm::make_vec3(in_ptr);
                            break;
                        case cgltf_attribute_type_normal:
                            v.normal = glm::make_vec3(in_ptr);
                            break;
                        case cgltf_attribute_type_texcoord:
                            v.uv0 = glm::make_vec2(in_ptr);
                            break;
                        case cgltf_attribute_type_tangent:
                            v.tangent = glm::make_vec4(in_ptr);
                            break;
                        default:
                            assert(!"Unhandled cgltf attribute type!");
                            break;
                        }
                    }
                    v.uv1 = uv / glm::vec2(atlas->width, atlas->height);
                    v.color = math::random_vector((u64)chart_index + 1337);
                    verts[i] = v;
                }

                for (u32 i = 0; i < mesh->indexCount; ++i)
                {
                    assert(mesh->indexArray[i] < mesh->vertexCount);
                    indices[i] = mesh->indexArray[i];
                }

                Primitive new_prim{};
                new_prim.index_count = (u32)prim->indices->count;
                new_prim.material = prim->material ? &materials[get_index(data->materials, prim->material)] : nullptr;

                {
                    u32 required_size = (u32)(sizeof(Vertex) * verts.count);
                    new_prim.vertex_buffer = ctx->allocate_buffer(required_size,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0
                    );

                    Vk_Allocated_Buffer vertex_staging = ctx->allocate_buffer(required_size,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                    );
                    void* mapped = 0;
                    vmaMapMemory(ctx->allocator, vertex_staging.allocation, &mapped);
                    memcpy(mapped, verts.data, required_size);
                    vmaUnmapMemory(ctx->allocator, vertex_staging.allocation);

                    VkBufferCopy buffer_copy = vkinit::buffer_copy(required_size, 0, 0);
                    vkCmdCopyBuffer(cmd, vertex_staging.buffer, new_prim.vertex_buffer.buffer, 1, &buffer_copy);
                }

                {
                    u32 required_size = (u32)(sizeof(u32) * indices.count);
                    new_prim.index_buffer = ctx->allocate_buffer(required_size,
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0
                    );

                    Vk_Allocated_Buffer staging = ctx->allocate_buffer(required_size,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                    );
                    void* mapped = 0;
                    vmaMapMemory(ctx->allocator, staging.allocation, &mapped);
                    memcpy(mapped, indices.data, required_size);
                    vmaUnmapMemory(ctx->allocator, staging.allocation);

                    VkBufferCopy buffer_copy = vkinit::buffer_copy(required_size, 0, 0);
                    vkCmdCopyBuffer(cmd, staging.buffer, new_prim.index_buffer.buffer, 1, &buffer_copy);
                }

                meshes[mesh_index].primitives.push_back(new_prim);
            }
        }

        vkEndCommandBuffer(cmd);
        VkSubmitInfo submit_info = vkinit::submit_info(1, &cmd);
        vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, 0);
        vkQueueWaitIdle(ctx->graphics_queue);

        // Get transforms from nodes
        for (u32 i = 0; i < data->nodes_count; ++i)
        {
            const cgltf_node* n = &data->nodes[i];
            if (n->mesh)
            {
                size_t mesh_index = get_index(data->meshes, n->mesh);
                cgltf_node_transform_world(n, (float*)&meshes[mesh_index].xform);
            }
        }

        {
            // Create lightmap texture
            lightmap_texture = ctx->allocate_image({ atlas->width, atlas->height, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

            VkCommandBuffer cmd = ctx->allocate_command_buffer();
            VkCommandBufferBeginInfo cmd_info = vkinit::command_buffer_begin_info();
            vkBeginCommandBuffer(cmd, &cmd_info);

            vkinit::vk_transition_layout(cmd, lightmap_texture.image,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                0, 0,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
            );

            vkEndCommandBuffer(cmd);

            VkSubmitInfo submit_info = vkinit::submit_info(1, &cmd);
            vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, 0);
            vkQueueWaitIdle(ctx->graphics_queue);
        }

        LOG_DEBUG("created atlas with %d meshes", atlas->meshCount);
    }

    {
        // Create pipelines

        // Raster pipelines
        Raster_Options opt{};
        opt.cull_mode = VK_CULL_MODE_NONE;
        opt.color_attachment_count = 1;
        opt.color_formats = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT };
        pipeline = ctx->create_raster_pipeline("shaders/spirv/lm_basic.vert.spv", "shaders/spirv/lm_basic.frag.spv", opt);

        opt.color_attachment_count = 2;
        opt.depth_test_enable = false;
        opt.depth_write_enable = false;
        lightmap_gbuffer_pipeline = ctx->create_raster_pipeline("shaders/spirv/lm_gbuf.vert.spv", "shaders/spirv/lm_gbuf.frag.spv", opt);

        // Raytracing pipelines
        VkDescriptorPoolCreateInfo cinfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        VkDescriptorPoolSize pool_sizes_bindless[] =
        {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_BINDLESS_RESOURCES },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BINDLESS_RESOURCES}
        };

        cinfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
        cinfo.maxSets = (u32)(std::size(pool_sizes_bindless) * MAX_BINDLESS_RESOURCES);
        cinfo.poolSizeCount = (u32)std::size(pool_sizes_bindless);
        cinfo.pPoolSizes = pool_sizes_bindless;

        vkCreateDescriptorPool(ctx->device, &cinfo, nullptr, &descriptor_pool);

        VkShaderStageFlags flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        // Bindless stuff
        VkDescriptorSetLayoutBinding bindless_bindings[] = {
            vkinit::descriptor_set_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_BINDLESS_RESOURCES, flags),
            vkinit::descriptor_set_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BINDLESS_RESOURCES, flags),
            vkinit::descriptor_set_layout_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BINDLESS_RESOURCES, flags),
            vkinit::descriptor_set_layout_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, flags),
            vkinit::descriptor_set_layout_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BINDLESS_RESOURCES, flags),
        };

        VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layout_info.bindingCount = uint32_t(std::size(bindless_bindings));
        layout_info.pBindings = bindless_bindings;
        layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

        VkDescriptorBindingFlags bindless_flags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT
            | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;

        VkDescriptorBindingFlags flags2[5] = { bindless_flags, bindless_flags, bindless_flags, 0, bindless_flags };

        VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT, nullptr };
        extended_info.bindingCount = (u32)std::size(bindless_bindings);
        extended_info.pBindingFlags = flags2;
        layout_info.pNext = &extended_info;

        VK_CHECK(vkCreateDescriptorSetLayout(ctx->device, &layout_info, nullptr, &bindless_set_layout));

        // Allocate bindless descriptors

        VkDescriptorSetAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        alloc_info.descriptorPool = descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &bindless_set_layout;

        VK_CHECK(vkAllocateDescriptorSets(ctx->device, &alloc_info, &bindless_descriptor_set));

        Shader rgen_shader, rmiss_shader, rchit_shader;
        load_shader_from_file(&rgen_shader, ctx->device, "shaders/spirv/lightmap.rgen.spv");
        load_shader_from_file(&rmiss_shader, ctx->device, "shaders/spirv/lightmap.rmiss.spv");
        load_shader_from_file(&rchit_shader, ctx->device, "shaders/spirv/lightmap.rchit.spv");

        VkDescriptorSetLayout push_desc_layout = ctx->create_descriptor_set_layout(1, &rgen_shader);
        VkDescriptorSetLayout layouts[] = { push_desc_layout, bindless_set_layout };
        lightmap_rt_pipeline = ctx->create_raytracing_pipeline(rgen_shader.shader, rmiss_shader.shader, rchit_shader.shader, layouts, (int)std::size(layouts));

        vkDestroyShaderModule(ctx->device, rgen_shader.shader, nullptr);
        vkDestroyShaderModule(ctx->device, rmiss_shader.shader, nullptr);
        vkDestroyShaderModule(ctx->device, rchit_shader.shader, nullptr);

        lightmap_rt_pipeline.pipeline.update_template = ctx->create_descriptor_update_template(1, &rgen_shader, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, lightmap_rt_pipeline.pipeline.layout);
    }

    {
        // Create render targets
        color_target.image = ctx->allocate_image({ window_width, window_height, 1 }, 
            VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        depth_target.image = ctx->allocate_image({ window_width, window_height, 1 }, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
        normal_target.image = ctx->allocate_image({ atlas->width, atlas->height, 1 },
            VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        position_target.image = ctx->allocate_image({ atlas->width, atlas->height, 1 },
            VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        lightmap_target.image = ctx->allocate_image({ atlas->width, atlas->height, 1 },
            VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        VkCommandBuffer cmd = ctx->allocate_command_buffer();
        VkCommandBufferBeginInfo begin_info = vkinit::command_buffer_begin_info();
        vkBeginCommandBuffer(cmd, &begin_info);

        vkinit::vk_transition_layout(cmd, depth_target.image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            0, 0,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            1, VK_IMAGE_ASPECT_DEPTH_BIT);

        vkinit::vk_transition_layout(cmd, lightmap_target.image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
            0, 0,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            1, VK_IMAGE_ASPECT_COLOR_BIT);

        color_target.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        normal_target.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        position_target.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        depth_target.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        lightmap_target.layout = VK_IMAGE_LAYOUT_GENERAL;
        
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit_info = vkinit::submit_info(1, &cmd);
        vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, 0);
        vkQueueWaitIdle(ctx->graphics_queue);
    }
}

void Lightmap_Renderer::set_camera(Camera_Component* camera)
{
    this->camera = camera;
}

void Lightmap_Renderer::render()
{
    u32 current_frame_index = frame_counter % FRAMES_IN_FLIGHT;
    u32 swapchain_image_index = 0;

    vkWaitForFences(ctx->device, 1, &ctx->frame_objects[current_frame_index].fence, VK_TRUE, UINT64_MAX);
    vkResetFences(ctx->device, 1, &ctx->frame_objects[current_frame_index].fence);
    vkAcquireNextImageKHR(ctx->device, ctx->swapchain,
        UINT64_MAX, ctx->frame_objects[current_frame_index].image_available_sem,
        VK_NULL_HANDLE, &swapchain_image_index);

    VkCommandBuffer cmd = ctx->frame_objects[current_frame_index].cmd;
    VkImage next_image = ctx->swapchain_images[swapchain_image_index];

    VkCommandBufferBeginInfo begin_info = vkinit::command_buffer_begin_info();
    vkBeginCommandBuffer(cmd, &begin_info);
    
    vkinit::vk_transition_layout(cmd, color_target.image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        1, VK_IMAGE_ASPECT_COLOR_BIT);

    vkinit::vk_transition_layout(cmd, normal_target.image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        1, VK_IMAGE_ASPECT_COLOR_BIT);

    vkinit::vk_transition_layout(cmd, position_target.image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        1, VK_IMAGE_ASPECT_COLOR_BIT);



    {
        // Render 3D view

        VkClearValue color_clear{};
        color_clear.color = { 0.0, 0.0, 0.0, 1.0 };
        VkClearValue depth_clear{};
        depth_clear.depthStencil.depth = 0.0f;

        VkRenderingAttachmentInfo color_info = vkinit::rendering_attachment_info(color_target.image.image_view, color_target.layout, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, color_clear);
        VkRenderingAttachmentInfo depth_info = vkinit::rendering_attachment_info(depth_target.image.image_view, depth_target.layout, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, depth_clear);

        VkRect2D render_area{};
        render_area.offset = { 0, 0 };
        render_area.extent = { window_width, window_height };
        VkRenderingAttachmentInfo color_infos[] = { color_info };
        VkRenderingInfo rendering_info = vkinit::rendering_info(render_area, (u32)std::size(color_infos), color_infos, &depth_info);

        vkCmdBeginRendering(cmd, &rendering_info);

        VkViewport viewport{};
        viewport.height = -(float)window_height;
        viewport.width = (float)window_width;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        viewport.x = 0.f;
        viewport.y = (float)window_height;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        vkCmdSetScissor(cmd, 0, 1, &render_area);

        glm::mat4 view_proj = camera ? camera->get_projection_matrix(aspect_ratio, 0.01f, 1000.f) * camera->get_view_matrix() : glm::mat4(1.0f);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
        vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(view_proj), &view_proj);

        for (const auto& m : meshes)
        {
            for (const auto& p : m.primitives)
            {
                vkCmdBindIndexBuffer(cmd, p.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::mat4), &m.xform);
                Descriptor_Info descs[] = {
                    Descriptor_Info(p.vertex_buffer.buffer, 0, VK_WHOLE_SIZE),
                    Descriptor_Info(default_sampler, lightmap_texture.image_view, VK_IMAGE_LAYOUT_GENERAL),
                    Descriptor_Info(block_sampler, lightmap_texture.image_view, VK_IMAGE_LAYOUT_GENERAL),
                };
                vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipeline.update_template, pipeline.layout, 0, &descs);
                vkCmdDrawIndexed(cmd, p.index_count, 1, 0, 0, 0);
            }
        }

        vkCmdEndRendering(cmd);
    }

    {
        // Render lightmap gbuffer

        VkClearValue clear{};
        clear.color = { 
            std::numeric_limits<float>::quiet_NaN(), 
            std::numeric_limits<float>::quiet_NaN(), 
            std::numeric_limits<float>::quiet_NaN(), 
            std::numeric_limits<float>::quiet_NaN() 
        };

        VkRenderingAttachmentInfo normal_info = vkinit::rendering_attachment_info(normal_target.image.image_view, normal_target.layout, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, clear);
        VkRenderingAttachmentInfo position_info = vkinit::rendering_attachment_info(position_target.image.image_view, position_target.layout, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, clear);

        VkRect2D render_area{};
        render_area.offset = { 0, 0 };
        render_area.extent = { atlas->width, atlas->height };
        VkRenderingAttachmentInfo color_infos[] = { normal_info, position_info };
        VkRenderingInfo rendering_info = vkinit::rendering_info(render_area, (u32)std::size(color_infos), color_infos);

        vkCmdBeginRendering(cmd, &rendering_info);

        VkViewport viewport{};
        viewport.height = -(float)atlas->height;
        viewport.width = (float)atlas->width;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        viewport.x = 0.f;
        viewport.y = (float)atlas->height;

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &render_area);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lightmap_gbuffer_pipeline.pipeline);

        for (const auto& m : meshes)
        {
            for (const auto& p : m.primitives)
            {
                vkCmdBindIndexBuffer(cmd, p.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdPushConstants(cmd, lightmap_gbuffer_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::mat4), &m.xform);
                Descriptor_Info descs[] = {
                    Descriptor_Info(p.vertex_buffer.buffer, 0, VK_WHOLE_SIZE)
                };
                vkCmdPushDescriptorSetWithTemplateKHR(cmd, lightmap_gbuffer_pipeline.update_template, lightmap_gbuffer_pipeline.layout, 0, &descs);
                vkCmdDrawIndexed(cmd, p.index_count, 1, 0, 0, 0);
            }
        }

        vkCmdEndRendering(cmd);
    }

    {
        // Raytrace lightmaps
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, lightmap_rt_pipeline.pipeline.pipeline);
        
        Descriptor_Info descs[] = {
                    Descriptor_Info(0, lightmap_target.image.image_view, VK_IMAGE_LAYOUT_GENERAL)
        };

        vkCmdPushDescriptorSetWithTemplateKHR(cmd, lightmap_rt_pipeline.pipeline.update_template, lightmap_rt_pipeline.pipeline.layout, 0, descs);

        //printf("Trace rays cmd at: %f\n", timer->get_current_time());
        vkCmdTraceRaysKHR(
            cmd,
            &lightmap_rt_pipeline.shader_binding_table.raygen_region,
            &lightmap_rt_pipeline.shader_binding_table.miss_region,
            &lightmap_rt_pipeline.shader_binding_table.chit_region,
            &lightmap_rt_pipeline.shader_binding_table.callable_region,
            atlas->width, atlas->height, 1
        );
    }

    VkImage copy_src = color_target.image.image;
    //VkImage copy_src = normal_target.image.image;
    //VkImage copy_src = lightmap_target.image.image;
    VkOffset3D src_extent = { (int)window_width, (int)window_height, 1 };
    VkImageLayout src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkinit::vk_transition_layout(cmd, copy_src, src_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        1, VK_IMAGE_ASPECT_COLOR_BIT);

    vkinit::vk_transition_layout(cmd, next_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        1, VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageSubresourceLayers src_res = vkinit::image_subresource_layers(VK_IMAGE_ASPECT_COLOR_BIT);
    VkImageSubresourceLayers dst_res = vkinit::image_subresource_layers(VK_IMAGE_ASPECT_COLOR_BIT);
    VkImageBlit2 blit2 = vkinit::image_blit2(src_res, { 0, 0, 0 }, src_extent, dst_res, { 0, 0, 0 }, { (int)window_width, (int)window_height, 1 });
    VkBlitImageInfo2 blit_info = vkinit::blit_image_info2(copy_src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, next_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit2);
    vkCmdBlitImage2(cmd, &blit_info);

    vkinit::vk_transition_layout(cmd, next_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_TRANSFER_WRITE_BIT, 0,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        1, VK_IMAGE_ASPECT_COLOR_BIT);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info = vkinit::submit_info(
        1, &cmd,
        1, &ctx->frame_objects[current_frame_index].image_available_sem,
        1, &ctx->frame_objects[current_frame_index].render_finished_sem
    );

    vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, ctx->frame_objects[current_frame_index].fence);

    VkPresentInfoKHR present_info = vkinit::present_info_khr(
        1, &ctx->frame_objects[current_frame_index].render_finished_sem,
        1, &ctx->swapchain, &swapchain_image_index
    );
        
    vkQueuePresentKHR(ctx->graphics_queue, &present_info);
}

void Lightmap_Renderer::shutdown()
{
    vkDestroySampler(ctx->device, default_sampler, nullptr);
    vkDestroySampler(ctx->device, block_sampler, nullptr);
    vkDestroyPipelineLayout(ctx->device, pipeline.layout, nullptr);
    vkDestroyPipeline(ctx->device, pipeline.pipeline, nullptr);
    vkDestroyDescriptorSetLayout(ctx->device, pipeline.desc_sets[0], nullptr);
    vkDestroyDescriptorUpdateTemplate(ctx->device, pipeline.update_template, nullptr);
    vkDestroyPipelineLayout(ctx->device, lightmap_gbuffer_pipeline.layout, nullptr);
    vkDestroyPipeline(ctx->device, lightmap_gbuffer_pipeline.pipeline, nullptr);
    vkDestroyDescriptorSetLayout(ctx->device, lightmap_gbuffer_pipeline.desc_sets[0], nullptr);
    vkDestroyDescriptorUpdateTemplate(ctx->device, lightmap_gbuffer_pipeline.update_template, nullptr);
    vkDestroyPipelineLayout(ctx->device, lightmap_rt_pipeline.pipeline.layout, nullptr);
    vkDestroyPipeline(ctx->device, lightmap_rt_pipeline.pipeline.pipeline, nullptr);
    vkDestroyDescriptorPool(ctx->device, descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(ctx->device, lightmap_rt_pipeline.pipeline.desc_sets[0], nullptr);
    vkDestroyDescriptorSetLayout(ctx->device, bindless_set_layout, nullptr);
    vkDestroyDescriptorUpdateTemplate(ctx->device, lightmap_rt_pipeline.pipeline.update_template, nullptr);
}

static void load_textures(Vk_Context* ctx, const char* basepath, cgltf_image* images, u32 image_count, std::vector<Vk_Allocated_Image>& out_images)
{
    constexpr int required_n_comps = 4; // GIVE ME 4 CHANNELS!!!

    VkCommandBuffer cmd = ctx->allocate_command_buffer();

    VkCommandBufferBeginInfo begin_info = vkinit::command_buffer_begin_info();
    vkBeginCommandBuffer(cmd, &begin_info);

    for (u32 i = 0; i < image_count; ++i)
    {
        GB_VALIDATE(images[i].mime_type == 0, "GLTF Buffer images not supported!\n");
        const char* fp = images[i].uri;
        stbi_set_flip_vertically_on_load(0);
        int x, y, comp;
        std::string path = std::string(basepath) + std::string(fp);
        stbi_uc* data = stbi_load(path.c_str(), &x, &y, &comp, required_n_comps);
        assert(data);

        u32 required_size = (x * y * required_n_comps) * sizeof(u8);
        Vk_Allocated_Image img = ctx->allocate_image(
            { (u32)x, (u32)y, 1 },
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_TILING_OPTIMAL
        );

        Vk_Allocated_Buffer staging_buffer = ctx->allocate_buffer(
            required_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        void* mapped;
        vmaMapMemory(ctx->allocator, staging_buffer.allocation, &mapped);
        memcpy(mapped, data, required_size);
        vmaUnmapMemory(ctx->allocator, staging_buffer.allocation);

        stbi_image_free(data);

        VkBufferImageCopy img_copy = vkinit::buffer_image_copy({ (u32)x, (u32)y, 1 });

        vkinit::vk_transition_layout(cmd, img.image,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT
        );

        vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &img_copy);

        vkinit::vk_transition_layout(cmd, img.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
            | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV);

        out_images.push_back(img);
    }

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info = vkinit::submit_info(1, &cmd);
    vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, 0);

    vkQueueWaitIdle(ctx->graphics_queue);
    ctx->free_command_buffer(cmd);
}

static xatlas::MeshDecl create_mesh_decl(cgltf_data* data, cgltf_primitive* prim)
{
    xatlas::MeshDecl decl{};
    decl.indexCount = (u32)prim->indices->count;
    decl.indexFormat = prim->indices->component_type == cgltf_component_type_r_16u ? xatlas::IndexFormat::UInt16 : xatlas::IndexFormat::UInt32;
    decl.indexData = (u8*)prim->indices->buffer_view->buffer->data +
        (u32)(prim->indices->offset + prim->indices->buffer_view->offset);

    cgltf_attribute* vertex_attrib = 0;
    for (u32 i = 0; i < prim->attributes_count; ++i)
    {
        if (prim->attributes[i].type == cgltf_attribute_type_position)
        {
            vertex_attrib = &prim->attributes[i];
            break;
        }
    }
    assert(vertex_attrib);
    decl.vertexCount = (u32)vertex_attrib->data->count;
    decl.vertexPositionData = (u8*)vertex_attrib->data->buffer_view->buffer->data
        + vertex_attrib->data->offset 
        + vertex_attrib->data->buffer_view->offset;
    decl.vertexPositionStride = (u32)vertex_attrib->data->stride;

    return decl;
}

static void render_debug_atlas(xatlas::Atlas* atlas, const char* filename)
{

    Image img{};
    img.w = atlas->width;
    img.h = atlas->height;

    img.data = (u8*)malloc(img.w * img.h * 3);
    assert(img.data);
    memset(img.data, 0, img.w * img.h * 3);

    for (uint32_t m = 0; m < atlas->meshCount; ++m)
    {
        xatlas::Mesh* mesh = &atlas->meshes[m];

        for (uint32_t tri = 0; tri < mesh->indexCount / 3; ++tri)
        {

            glm::vec2 uvs[3];
            int chart_index = mesh->vertexArray[mesh->indexArray[tri * 3]].chartIndex;
            glm::vec3 color = math::random_vector((u64)chart_index + 1337);
            for (uint32_t i = 0; i < 3; ++i)
            {
                uint32_t index = mesh->indexArray[tri * 3 + i];
                uvs[i].x = mesh->vertexArray[index].uv[0];
                uvs[i].y = mesh->vertexArray[index].uv[1];
            }
            fill_triangle(&img, uvs, color);
            draw_triangle(&img, uvs, glm::vec3(1.0));
        }
    }

    write_image(&img, filename);
}

static void fill_triangle(Image* img, glm::vec2 uvs[3], glm::vec3 color)
{
    glm::vec2 v0 = uvs[0];
    glm::vec2 v1 = uvs[1];
    glm::vec2 v2 = uvs[2];
    // Compute triangle bounding box
    int minX = (int)min3(v0.x, v1.x, v2.x);
    int minY = (int)min3(v0.y, v1.y, v2.y);
    int maxX = (int)max3(v0.x, v1.x, v2.x);
    int maxY = (int)max3(v0.y, v1.y, v2.y);

    // Clip against screen bounds
    minX = std::max(minX, 0);
    minY = std::max(minY, 0);
    maxX = std::min(maxX, (i32)img->w - 1);
    maxY = std::min(maxY, (i32)img->h - 1);

    // Rasterize
    glm::ivec2 p;
    for (p.y = minY; p.y <= maxY; p.y++) {
        for (p.x = minX; p.x <= maxX; p.x++) {
            // Determine barycentric coordinates
            int w0 = orient2d(v1, v2, p);
            int w1 = orient2d(v2, v0, p);
            int w2 = orient2d(v0, v1, p);

            // If p is on or inside all edges, render pixel.
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                set_pixel(img, (int)p.x, (int)p.y, color);
            if (w0 <= 0 && w1 <= 0 && w2 <= 0)
                set_pixel(img, (int)p.x, (int)p.y, color);
        }
    }
}

static void draw_triangle(Image* img, glm::vec2 uvs[3], glm::vec3 color)
{
    for (int i = 0; i < 3; ++i)
    {
        int x0 = (int)uvs[i].x;
        int y0 = (int)uvs[i].y;
        int x1 = (int)uvs[(i + 1) % 3].x;
        int y1 = (int)uvs[(i + 1) % 3].y;

        draw_line(x0, y0, x1, y1, img, color);
    }
}

static void draw_line(int x0, int y0, int x1, int y1, Image* img, glm::vec3 color) {
    bool steep = false;
    if (std::abs(x0 - x1) < std::abs(y0 - y1)) {
        std::swap(x0, y0);
        std::swap(x1, y1);
        steep = true;
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    int dx = x1 - x0;
    int dy = y1 - y0;
    int derror2 = std::abs(dy) * 2;
    int error2 = 0;
    int y = y0;
    for (int x = x0; x <= x1; x++) {
        if (steep) {
            set_pixel(img, y, x, color);
        }
        else {
            set_pixel(img, x, y, color);
        }
        error2 += derror2;
        if (error2 > dx) {
            y += (y1 > y0 ? 1 : -1);
            error2 -= dx * 2;
        }
    }
}

static void set_pixel(Image* img, int x, int y, glm::vec3 color)
{
    if (x < 0 || x >= (i32)img->w || y < 0 || y >= (i32)img->h)
        return;

    uint8_t r = uint8_t(color.r * 255.0f);
    uint8_t g = uint8_t(color.g * 255.0f);
    uint8_t b = uint8_t(color.b * 255.0f);

    img->data[(y * img->w + x) * 3 + 0] = r;
    img->data[(y * img->w + x) * 3 + 1] = g;
    img->data[(y * img->w + x) * 3 + 2] = b;
}

static int orient2d(glm::vec2 a, glm::vec2 b, glm::vec2 c)
{
    return (int)((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
}

static void write_image(Image* img, const char* filename)
{
    stbi_write_png(filename, img->w, img->h, 3, img->data, 0);
}

} //namespace lm