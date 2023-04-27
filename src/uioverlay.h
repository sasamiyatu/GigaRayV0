#pragma once
#include "r_vulkan.h"
#include "imgui/imgui.h"

struct UI_Overlay
{

	void initialize(Vk_Context* ctx, u32 window_width, u32 window_height, Vk_Allocated_Image* render_attachment, SDL_Window* window);
	void update_and_render(VkCommandBuffer cmd, float dt);
	void shutdown();

	struct Push_Constant_Block
	{
		glm::vec2 scale;
		glm::vec2 translate;
	} push_constant_block;

	Vk_Context* ctx;
	Vk_Allocated_Image font_image;
	VkSampler sampler;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet descriptor_set;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	Vk_Allocated_Image* render_attachment;
	u32 window_width, window_height;
	Vk_Allocated_Buffer vertex_buffer;
	Vk_Allocated_Buffer index_buffer;
	ImDrawVert* vertex_dst;
	ImDrawIdx* index_dst;
	float smoothed_delta = 1.0f / 60.0f;
	float scale = 1.0f;
	bool test_val = false;
	std::vector<const char*> test_combo = {
		"pepega",
		"nigga",
		"broski"
	};
	int current_item = 0;
};