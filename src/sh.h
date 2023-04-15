#pragma once
#include "defines.h"
#include "r_vulkan.h"
#include "r_mesh.h"

struct Scene;

struct SH_2
{
    glm::vec3 coefs[9];
};

struct Probe_System
{
    SH_2 probe; // Single probe for now
    Vk_Context* ctx;
    Vk_Allocated_Buffer probe_samples;
    Vk_Pipeline sh_integrate_pipeline;
    Vk_Pipeline sh_debug_rendering_pipeline;
    SH_2* mapped_probe_data;
    glm::vec3 bbmin;
    glm::vec3 bbmax;
    glm::uvec3 probe_counts;
    VkDescriptorSet bindless_descriptor_set;
    GPU_Buffer* gpu_camera_data;
    Scene* scene;

    const float probe_spacing = 100.0;
    const u32 samples_per_pass = 256;
    const u32 max_samples = 16384;
    u32 samples_accumulated = 0;

    Mesh debug_mesh;

    void init(Vk_Context* ctx, VkDescriptorSet bindless_descriptor_set, VkDescriptorSetLayout bindless_set_layout, GPU_Buffer* gpu_camera_data, Scene* scene, VkCommandBuffer cmd);
    void init_probe_grid(glm::vec3 min, glm::vec3 max);
    void bake(VkCommandBuffer cmd, Cubemap* envmap, VkSampler sampler);
    void debug_render(VkCommandBuffer cmd);
    void shutdown();
};