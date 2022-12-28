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
static void load_buffers(Vk_Context* ctx, const std::vector<Raw_Buffer>& raw_buffers, std::vector<Vk_Allocated_Buffer>& out_buffers);
static void load_raw_buffers(const char* basepath, cgltf_buffer* buffers, u32 buffer_count, std::vector<Raw_Buffer>& raw_buffers);
static xatlas::MeshDecl create_mesh_decl(cgltf_data* data, cgltf_primitive* prim, std::vector<Raw_Buffer>& raw_buffers);
static void render_debug_atlas(xatlas::Atlas* atlas, const char* filename);
static void fill_triangle(Image* img, glm::vec2 uvs[3], glm::vec3 color);
static int orient2d(glm::vec2 a, glm::vec2 b, glm::vec2 c);
static void draw_triangle(Image* img, glm::vec2 uvs[3], glm::vec3 color);
static void draw_line(int x0, int y0, int x1, int y1, Image* img, glm::vec3 color);
static void set_pixel(Image* img, int x, int y, glm::vec3 color);
static void write_image(Image* img, const char* filename);

Lightmap_Renderer::Lightmap_Renderer(Vk_Context* context)
    : ctx(context), 
    atlas(nullptr),
    scene_data(nullptr)
{
}

Lightmap_Renderer::~Lightmap_Renderer()
{
    vkDestroySampler(ctx->device, default_sampler, nullptr);
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
    scene_data = data;

    u32 str_length = (u32)strlen(gltf_path);
    char* stripped = (char*)malloc(str_length + 1);
    strcpy(stripped, gltf_path);
    char* slashpos;
    for (slashpos = stripped + str_length - 1; slashpos && *slashpos != '/'; --slashpos)
        ;
    *(slashpos + 1) = 0;

    load_raw_buffers(stripped, data->buffers, (u32)data->buffers_count, raw_buffers);
    load_textures(ctx, stripped, data->images, (u32)data->images_count, images);
    load_buffers(ctx, raw_buffers, buffers);

    // Create textures
    VkSamplerCreateInfo sampler_info = vkinit::sampler_create_info();
    vkCreateSampler(ctx->device, &sampler_info, nullptr, &default_sampler);
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

    // Create meshes
    for (u32 m = 0; m < data->meshes_count; ++m)
    {
        Mesh new_mesh{};
        for (u32 p = 0; p < data->meshes[m].primitives_count; ++p)
        {
            cgltf_primitive* prim = &data->meshes[m].primitives[p];
            Primitive new_prim{};

            Index_Info info{};
            assert(prim->indices->component_type == cgltf_component_type_r_16u ||
                prim->indices->component_type == cgltf_component_type_r_32u);
            info.index_type = prim->indices->component_type == cgltf_component_type_r_16u ?
                Index_Info::Index_Type::UINT16 : Index_Info::Index_Type::UINT32;
            info.count = (u32)prim->indices->count;
            info.buffer = buffers[get_index(data->buffers, prim->indices->buffer_view->buffer)].buffer;
            info.offset = u32(prim->indices->offset + prim->indices->buffer_view->offset);
            info.stride = (u32)prim->indices->stride;
            new_prim.indices = info;

            for (u32 a = 0; a < prim->attributes_count; ++a)
            {
                Attribute attrib{};
                cgltf_accessor* acc = prim->attributes[a].data;
                attrib.count = (u32)acc->count;
                attrib.offset = u32(acc->offset + acc->buffer_view->offset);
                attrib.buffer = buffers[get_index(data->buffers, acc->buffer_view->buffer)].buffer;
                new_prim.material = &materials[get_index(data->materials, prim->material)];
                switch (prim->attributes[a].type)
                {
                case cgltf_attribute_type_normal:
                    assert(acc->component_type == cgltf_component_type_r_32f);
                    assert(acc->type == cgltf_type_vec3);
                    new_prim.normal = attrib;
                    break;
                case cgltf_attribute_type_position:
                    assert(acc->component_type == cgltf_component_type_r_32f);
                    assert(acc->type == cgltf_type_vec3);
                    new_prim.position = attrib;
                    break;
                case cgltf_attribute_type_texcoord:
                    assert(acc->component_type == cgltf_component_type_r_32f);
                    assert(acc->type == cgltf_type_vec2);
                    new_prim.texcoord0 = attrib;
                    break;
                case cgltf_attribute_type_tangent:
                    assert(acc->component_type == cgltf_component_type_r_32f);
                    assert(acc->type == cgltf_type_vec4);
                    new_prim.tangent = attrib;
                    break;
                default:
                    assert(!"Attribute type not implemented\n");
                }
            }
            new_mesh.primitives.push_back(new_prim);

        }
        meshes.push_back(new_mesh);
    }

    // Generate atlas
    atlas = xatlas::Create();
    for (u32 m = 0; m < data->meshes_count; ++m)
    {
        for (u32 p = 0; p < data->meshes[m].primitives_count; ++p)
        {
            xatlas::MeshDecl decl = create_mesh_decl(data, &data->meshes[m].primitives[p], raw_buffers);
            xatlas::AddMeshError err = xatlas::AddMesh(atlas, decl);
            assert(err == xatlas::AddMeshError::Success);
        }
    }

    xatlas::Generate(atlas);
    render_debug_atlas(atlas, "test_atlas.png");

    LOG_DEBUG("created atlas with %d meshes", atlas->meshCount);

    // Create pipelines
    Shader vert_shader{};
    bool success = load_shader_from_file(&vert_shader, ctx->device, "shaders/spirv/lm_basic.vert.spv");
    Shader frag_shader{};
    success = load_shader_from_file(&frag_shader, ctx->device, "shaders/spirv/lm_basic.frag.spv");

    Shader shaders[] = { vert_shader, frag_shader };
    VkDescriptorSetLayout desc_set_layout = ctx->create_descriptor_set_layout((u32)std::size(shaders), shaders);
    pipeline = ctx->create_raster_pipeline(vert_shader.shader, frag_shader.shader, 1, &desc_set_layout);
    pipeline.update_template = ctx->create_descriptor_update_template((u32)std::size(shaders), shaders, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout);
    pipeline.desc_sets[0] = desc_set_layout;

    // Create render targets

}

void Lightmap_Renderer::render()
{
    
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

static void load_buffers(Vk_Context* ctx, const std::vector<Raw_Buffer>& raw_buffers, std::vector<Vk_Allocated_Buffer>& out_buffers)
{
    VkCommandBuffer cmd = ctx->allocate_command_buffer();

    VkCommandBufferBeginInfo begin_info = vkinit::command_buffer_begin_info();
    vkBeginCommandBuffer(cmd, &begin_info);

    for (size_t i = 0; i < raw_buffers.size(); ++i)
    {
        u8* data = raw_buffers[i].data;
        u32 size = raw_buffers[i].size;

        Vk_Allocated_Buffer gpu_buffer = ctx->allocate_buffer(
            size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

        Vk_Allocated_Buffer staging_buffer = ctx->allocate_buffer(
            size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        void* mapped;
        vmaMapMemory(ctx->allocator, staging_buffer.allocation, &mapped);
        memcpy(mapped, data, size);
        vmaUnmapMemory(ctx->allocator, staging_buffer.allocation);

        VkBufferCopy buffer_copy = vkinit::buffer_copy(size);
        vkCmdCopyBuffer(cmd, staging_buffer.buffer, gpu_buffer.buffer, 1, &buffer_copy);

        out_buffers.push_back(gpu_buffer);
    }

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info = vkinit::submit_info(1, &cmd);
    vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, 0);

    vkQueueWaitIdle(ctx->graphics_queue);
    ctx->free_command_buffer(cmd);
}

static void load_raw_buffers(const char* basepath, cgltf_buffer* buffers, u32 buffer_count, std::vector<Raw_Buffer>& raw_buffers)
{
    for (u32 i = 0; i < buffer_count; ++i)
    {
        Raw_Buffer buf{};
        std::string fp = std::string(basepath) + std::string(buffers->uri);
        u32 bytes_read = read_entire_file(fp.c_str(), &buf.data);
        buf.size = bytes_read;
        raw_buffers.push_back(buf);
    }
}

static xatlas::MeshDecl create_mesh_decl(cgltf_data* data, cgltf_primitive* prim, std::vector<Raw_Buffer>& raw_buffers)
{
    xatlas::MeshDecl decl{};
    decl.indexCount = (u32)prim->indices->count;
    decl.indexFormat = prim->indices->component_type == cgltf_component_type_r_16u ? xatlas::IndexFormat::UInt16 : xatlas::IndexFormat::UInt32;
    decl.indexData = raw_buffers[get_index(data->buffers, prim->indices->buffer_view->buffer)].data + 
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
    decl.vertexPositionData = 
        raw_buffers[get_index(data->buffers, vertex_attrib->data->buffer_view->buffer)].data 
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
            glm::vec3 color = math::random_vector((u64)chart_index);
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