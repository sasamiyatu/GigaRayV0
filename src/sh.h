#pragma once
#include "defines.h"
#include "r_vulkan.h"
#include "r_mesh.h"

struct Vk_Context;

struct SH_3
{
    glm::vec3 coefs[9];
};

struct Probe_System
{
    SH_3 probe; // Single probe for now
    Vk_Context* ctx;
    Vk_Allocated_Buffer probe_samples;
    Vk_Pipeline sh_integrate_pipeline;
    SH_3* mapped_probe_data;
    glm::vec3 bbmin;
    glm::vec3 bbmax;
    VkDescriptorSet bindless_descriptor_set;

    const float probe_spacing = 5.0;

    Mesh debug_mesh;

    void init(Vk_Context* ctx, VkDescriptorSet bindless_descriptor_set, VkDescriptorSetLayout bindless_set_layout);
    void init_probe_grid(glm::vec3 min, glm::vec3 max);
    void bake(VkCommandBuffer cmd, Cubemap* envmap, VkSampler sampler);
};