#include "uioverlay.h"
#include "imgui/imgui.h"
#include "vk_helpers.h"
#include "shaders.h"
#include "imgui/imgui_impl_sdl2.h"
#include "settings.h"

#define MAX_VERTEX_COUNT 65536
#define MAX_INDEX_COUNT 65536

void UI_Overlay::initialize(Vk_Context* ctx, u32 window_width, u32 window_height, Vk_Allocated_Image* render_attachment, SDL_Window* window)
{
	this->ctx = ctx;
	this->window_width = window_width;
	this->window_height = window_height;
	this->render_attachment = render_attachment;

	// Init ImGui
	ImGui::CreateContext();
	// Color scheme
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
	style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
	// Dimensions
	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = 1.0f;

	ImGui_ImplSDL2_InitForVulkan(window);

	// Create font texture
	unsigned char* font_data;
	int tex_width, tex_height;
	io.Fonts->AddFontFromFileTTF("data/fonts/Roboto-Medium.ttf", 16.0f);
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);
	VkDeviceSize upload_size = tex_width * tex_height * 4 * sizeof(char);

	font_image = ctx->allocate_image({ (u32)tex_width, (u32)tex_height, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	Vk_Allocated_Buffer staging_buffer = ctx->allocate_buffer((u32)upload_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 0);

	void* mapped;
	vmaMapMemory(ctx->allocator, staging_buffer.allocation, &mapped);
	memcpy(mapped, font_data, upload_size);
	vmaUnmapMemory(ctx->allocator, staging_buffer.allocation);

	VkCommandBuffer copy_cmd = ctx->allocate_command_buffer();

	VkCommandBufferBeginInfo begin_info = vkinit::command_buffer_begin_info();

	vkBeginCommandBuffer(copy_cmd, &begin_info);
	
	vkinit::set_image_layout(
		copy_cmd,
		font_image.image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT);

	VkBufferImageCopy buffer_copy = {};
	buffer_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	buffer_copy.imageSubresource.layerCount = 1;
	buffer_copy.imageExtent = { (u32)tex_width, (u32)tex_height, 1 };

	vkCmdCopyBufferToImage(
		copy_cmd,
		staging_buffer.buffer,
		font_image.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&buffer_copy
	);

	vkinit::set_image_layout(
		copy_cmd,
		font_image.image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT);

	vkEndCommandBuffer(copy_cmd);

	VkSubmitInfo submit_info = vkinit::submit_info(1, &copy_cmd);
	VkFenceCreateInfo fence_info = vkinit::fence_create_info();
	VkFence fence;
	VK_CHECK(vkCreateFence(ctx->device, &fence_info, nullptr, &fence));
	VK_CHECK(vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, fence));
	VK_CHECK(vkWaitForFences(ctx->device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
	vkDestroyFence(ctx->device, fence, nullptr);

	vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &copy_cmd);

	// Font texture sampler
	VkSamplerCreateInfo sampler_info = vkinit::sampler_create_info();
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	VK_CHECK(vkCreateSampler(ctx->device, &sampler_info, nullptr, &sampler));

	// Descriptor pool
	VkDescriptorPoolSize pool_sizes[] = {
		vkinit::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
	};

	VkDescriptorPoolCreateInfo descriptor_pool_info = vkinit::descriptor_pool_create_info(pool_sizes, (u32)std::size(pool_sizes), 2);
	VK_CHECK(vkCreateDescriptorPool(ctx->device, &descriptor_pool_info, nullptr, &descriptor_pool));

	// Descriptor set layout
	VkDescriptorSetLayoutBinding layout_bindings[] = {
		vkinit::descriptor_set_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
	};
	VkDescriptorSetLayoutCreateInfo descriptor_layout = vkinit::descriptor_set_layout_create_info(layout_bindings, (u32)std::size(layout_bindings));
	VK_CHECK(vkCreateDescriptorSetLayout(ctx->device, &descriptor_layout, nullptr, &descriptor_set_layout));

	// Descriptor set
	VkDescriptorSetAllocateInfo alloc_info = vkinit::descriptor_set_allocate_info(descriptor_pool, &descriptor_set_layout, 1);
	VK_CHECK(vkAllocateDescriptorSets(ctx->device, &alloc_info, &descriptor_set));
	VkDescriptorImageInfo font_descriptor = vkinit::descriptor_image_info(sampler, font_image.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkWriteDescriptorSet write_descriptor_sets[] = {
		vkinit::write_descriptor_set(&font_descriptor, 0, 0, descriptor_set)
	};
	vkUpdateDescriptorSets(ctx->device, (u32)std::size(write_descriptor_sets), write_descriptor_sets, 0, nullptr);

	// Create pipeline

	// Pipeline layout
	VkPushConstantRange push_constant_range{};
	push_constant_range.offset = 0;
	push_constant_range.size = sizeof(Push_Constant_Block);
	push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	
	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info(1, &descriptor_set_layout, 1, &push_constant_range);
	VK_CHECK(vkCreatePipelineLayout(ctx->device, &pipeline_layout_info, nullptr, &pipeline_layout));

	// Setup graphics pipeline
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state = vkinit::pipeline_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterization_state = vkinit::pipeline_rasterization_state_create_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

	VkPipelineColorBlendAttachmentState blend_state = {};
	blend_state.blendEnable = VK_TRUE;
	blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_state.colorBlendOp = VK_BLEND_OP_ADD;
	blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blend_state.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo color_blend_state = vkinit::pipeline_color_blend_state_create_info(1, &blend_state);
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state = vkinit::pipeline_depth_stencil_state_create_info(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
	VkPipelineViewportStateCreateInfo viewport_state = vkinit::pipeline_viewport_state_create_info(1, nullptr, 1, nullptr);
	VkPipelineMultisampleStateCreateInfo multisample_state = vkinit::pipeline_multisample_state_create_info(VK_SAMPLE_COUNT_1_BIT);

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state = vkinit::pipeline_dynamic_state_create_info((u32)std::size(dynamic_states), dynamic_states);

	Shader vertex, fragment;

	load_shader_from_file(&vertex, ctx->device, "shaders/spirv/uioverlay.vert.spv");
	load_shader_from_file(&fragment, ctx->device, "shaders/spirv/uioverlay.frag.spv");

	VkPipelineShaderStageCreateInfo vert_info = vkinit::pipeline_shader_stage_create_info(vertex.shader, vertex.shader_stage, "main");
	VkPipelineShaderStageCreateInfo frag_info = vkinit::pipeline_shader_stage_create_info(fragment.shader, fragment.shader_stage, "main");

	VkPipelineShaderStageCreateInfo shader_infos[] = { vert_info, frag_info };

	VkFormat color_attachment_format = VK_FORMAT_B8G8R8A8_UNORM;
	VkPipelineRenderingCreateInfo rendering_create_info{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	rendering_create_info.viewMask = 0;
	rendering_create_info.colorAttachmentCount = 1;
	rendering_create_info.pColorAttachmentFormats = &color_attachment_format;
	rendering_create_info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

	VkGraphicsPipelineCreateInfo pipeline_create_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipeline_create_info.pInputAssemblyState = &input_assembly_state;
	pipeline_create_info.pRasterizationState = &rasterization_state;
	pipeline_create_info.pColorBlendState = &color_blend_state;
	pipeline_create_info.pMultisampleState = &multisample_state;
	pipeline_create_info.pViewportState = &viewport_state;
	pipeline_create_info.pDepthStencilState = &depth_stencil_state;
	pipeline_create_info.pDynamicState = &dynamic_state;
	pipeline_create_info.stageCount = (u32)std::size(shader_infos);
	pipeline_create_info.pStages = shader_infos;
	pipeline_create_info.layout = pipeline_layout;
	pipeline_create_info.pNext = &rendering_create_info;

	VkVertexInputBindingDescription vertex_input_bindings[] = {
		vkinit::vertex_input_binding_description(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX)
	};

	VkVertexInputAttributeDescription vertex_input_attributes[] = {
		vkinit::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),
		vkinit::vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),
		vkinit::vertex_input_attribute_description(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_state = vkinit::pipeline_vertex_input_state_create_info(
		(u32)std::size(vertex_input_bindings), vertex_input_bindings,
		(u32)std::size(vertex_input_attributes), vertex_input_attributes);

	pipeline_create_info.pVertexInputState = &vertex_input_state;

	VK_CHECK(vkCreateGraphicsPipelines(ctx->device, 0, 1, &pipeline_create_info, nullptr, &pipeline));

	vkDestroyShaderModule(ctx->device, vertex.shader, nullptr);
	vkDestroyShaderModule(ctx->device, fragment.shader, nullptr);


	// Allocate index and vertex buffers
	VkDeviceSize vertex_buffer_size = MAX_VERTEX_COUNT * sizeof(ImDrawVert);
	VkDeviceSize index_buffer_size = MAX_INDEX_COUNT * sizeof(ImDrawIdx);

	vertex_buffer = ctx->allocate_buffer((u32)vertex_buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 0);
	index_buffer = ctx->allocate_buffer((u32)index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 0);

	vmaMapMemory(ctx->allocator, vertex_buffer.allocation, &mapped);
	vertex_dst = (ImDrawVert*)mapped;
	vmaMapMemory(ctx->allocator, index_buffer.allocation, &mapped);
	index_dst = (ImDrawIdx*)mapped;
}

void UI_Overlay::update_and_render(VkCommandBuffer cmd, float dt)
{
	smoothed_delta = glm::mix(dt, smoothed_delta, 0.95f);

	// Update overlay

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)window_width, (float)window_height);
	io.DeltaTime = dt;

	int mouse_x, mouse_y;
	u32 mouse_state = SDL_GetMouseState(&mouse_x, &mouse_y);
	
	io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
	io.MouseDown[0] = (mouse_state & SDL_BUTTON(1)) != 0;
	io.MouseDown[1] = (mouse_state & SDL_BUTTON(3)) != 0;

	ImGui::NewFrame();

	//ImGui::ShowDemoWindow();
	if (g_settings.menu_open)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
		ImGui::SetNextWindowPos(ImVec2(10, 10));
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
		ImGui::Begin("GigaRay", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
		ImGui::TextUnformatted("GigaRay");
		ImGui::TextUnformatted(ctx->physical_device_properties.properties.deviceName);
		ImGui::Text("%.2f ms/frame (%.1d fps)", smoothed_delta * 1000.0f, (int)std::round(1.0f / smoothed_delta));

		ImGui::PushItemWidth(110.0f * scale);
		//OnUpdateUIOverlay(&UIOverlay);
		//bool res = ImGui::Checkbox("caption", &test_val);
		//bool changed = ImGui::Combo("TestCombo", &current_item, test_combo.data(), (int)test_combo.size(), -1);
		bool is_unfolded = ImGui::CollapsingHeader("Sunlight", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen);
		if (is_unfolded)
		{
			ImGui::SliderFloat("Sun azimuth (deg)", &g_settings.sun_azimuth, 0.0f, 360.0f, "%.2f");
			ImGui::SliderFloat("Sun zenith (deg)", &g_settings.sun_zenith, 0.0f, 180.0f, "%.2f");
			ImGui::SliderFloat("Sun intensity", &g_settings.sun_intensity, 0.0f, 999.0f, "%.1f");
		}
		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen))
		{
			static const char* on_screen_modes[] = { "Final output", "Indirect diffuse", "Denoised", "Hit distance", "Blur radius", "History length"};
			ImGui::SliderFloat("Exposure", &g_settings.exposure, 0.0f, 100.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::Combo("On screen", &g_settings.screen_output, on_screen_modes, (int)std::size(on_screen_modes));
		}
		if (ImGui::CollapsingHeader("Denoising", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen))
		{
			static const char* temporal_filter_modes[] = { "Bilinear", "Bicubic" };
			ImGui::SliderFloat("Pre-pass blur radius", &g_settings.prepass_blur_radius, 0.0f, 100.0f, "%.0f");
			ImGui::Checkbox("Temporal accumulation", &g_settings.temporal_accumulation);
			ImGui::Checkbox("History fix", &g_settings.history_fix);
			ImGui::SliderFloat2("Hit distance params", (float*)&g_settings.hit_distance_params, 0.001f, 100.0f, "%.3f");
			ImGui::Combo("Temporal filter", (int*)&g_settings.temporal_filter, temporal_filter_modes, (int)std::size(temporal_filter_modes));
			ImGui::SliderFloat("Bicubic sharpness", &g_settings.bicubic_sharpness, 0.0f, 1.0f, "%.2f");
		}

		ImGui::PopItemWidth();

		ImGui::End();
		ImGui::PopStyleVar();
	}

	ImGui::Render();


	ImDrawData* imDrawData = ImGui::GetDrawData();

	if (!imDrawData || imDrawData->CmdListsCount == 0) return;

	// Update vertex / index buffers

	assert(imDrawData->TotalVtxCount < MAX_VERTEX_COUNT);
	assert(imDrawData->TotalIdxCount < MAX_INDEX_COUNT);

	ImDrawVert* vtx_dst = vertex_dst;
	ImDrawIdx* idx_dist = index_dst;
	for (int i = 0; i < imDrawData->CmdListsCount; ++i)
	{
		const ImDrawList* cmd_list = imDrawData->CmdLists[i];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dist, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dist += cmd_list->IdxBuffer.Size;
	}

	vmaFlushAllocation(ctx->allocator, vertex_buffer.allocation, 0, VK_WHOLE_SIZE);
	vmaFlushAllocation(ctx->allocator, index_buffer.allocation, 0, VK_WHOLE_SIZE);

	// Render the UI

	VkRect2D render_area{};
	render_area.extent = { window_width, window_height };
	render_area.offset = { 0, 0 };

	VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	attachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	attachment.imageView = render_attachment->image_view;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	VkRenderingInfo rendering_info = vkinit::rendering_info(render_area, 1, &attachment);
	
	vkCmdBeginRendering(cmd, &rendering_info);

	VkViewport viewport{};
	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;
	viewport.width = (float)window_width;
	viewport.height = (float)window_height;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

	push_constant_block.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
	push_constant_block.translate = glm::vec2(-1.0f);
	vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push_Constant_Block), &push_constant_block);

	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer.buffer, offsets);
	vkCmdBindIndexBuffer(cmd, index_buffer.buffer, 0, VK_INDEX_TYPE_UINT16);

	i32 vertex_offset = 0;
	i32 index_offset = 0;

	for (i32 i = 0; i < imDrawData->CmdListsCount; ++i)
	{
		const ImDrawList* cmd_list = imDrawData->CmdLists[i];
		for (i32 j = 0; j < cmd_list->CmdBuffer.Size; ++j)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
			VkRect2D scissor_rect;
			scissor_rect.offset.x = std::max((i32)(pcmd->ClipRect.x), 0);
			scissor_rect.offset.y = std::max((i32)(pcmd->ClipRect.y), 0);
			scissor_rect.extent.width = (u32)(pcmd->ClipRect.z - pcmd->ClipRect.x);
			scissor_rect.extent.height = (u32)(pcmd->ClipRect.w - pcmd->ClipRect.y);
			vkCmdSetScissor(cmd, 0, 1, &scissor_rect);
			vkCmdDrawIndexed(cmd, pcmd->ElemCount, 1, index_offset, vertex_offset, 0);
			index_offset += pcmd->ElemCount;
		}
		vertex_offset += cmd_list->VtxBuffer.Size;
	}

	vkCmdEndRendering(cmd);
}

void UI_Overlay::shutdown()
{
	if (ImGui::GetCurrentContext()) 
	{
		ImGui::DestroyContext();
	}

	vmaUnmapMemory(ctx->allocator, vertex_buffer.allocation);
	vmaUnmapMemory(ctx->allocator, index_buffer.allocation);

	vkDestroyDescriptorSetLayout(ctx->device, descriptor_set_layout, nullptr);
	vkDestroyDescriptorPool(ctx->device, descriptor_pool, nullptr);
	vkDestroySampler(ctx->device, sampler, nullptr);
	vkDestroyPipeline(ctx->device, pipeline, nullptr);
	vkDestroyPipelineLayout(ctx->device, pipeline_layout, nullptr);
}
