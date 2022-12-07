#pragma once
#include "r_vulkan.h"
#include "defines.h"

struct Render_Target;

struct Gbuffer_Renderer
{
	Vk_Context* ctx;
	
	Render_Target* albedo;
	Render_Target* normal;
	Render_Target* world_pos;
	Render_Target* depth;

	VkDescriptorPool descriptor_pool;

	Gbuffer_Renderer(Vk_Context* ctx);

	void initialize(
		Render_Target* albedo_rt,
		Render_Target* normal_rt,
		Render_Target* world_pos_rt,
		Render_Target* depth_rt
	);
	void run();
};