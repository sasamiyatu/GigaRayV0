#pragma once
#include "defines.h"
#include "r_vulkan.h"


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

    void init(Vk_Context* ctx);
    void bake(VkCommandBuffer cmd, Cubemap* envmap, VkSampler sampler);
};