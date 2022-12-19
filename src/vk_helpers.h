#pragma once
#include "Volk/volk.h"

namespace vkinit
{
	inline VkAccelerationStructureInstanceKHR acceleration_structure_instance(uint64_t acceleration_structure_ref)
	{
		VkAccelerationStructureInstanceKHR instance{};
		instance.transform.matrix[0][0] = instance.transform.matrix[1][1] = instance.transform.matrix[2][2] = 1.f;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = acceleration_structure_ref;

		return instance;
	}

	// Inserts a barrier into the command buffer to perform a specific layout transition
	inline void vk_transition_layout(
		VkCommandBuffer cmd,
		VkImage img,
		VkImageLayout src_layout,
		VkImageLayout dst_layout,
		VkAccessFlags src_access_mask,
		VkAccessFlags dst_access_mask,
		VkPipelineStageFlags src_stage_mask,
		VkPipelineStageFlags dst_stage_mask,
		int mip_levels = 1,
		VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT)
	{
		// Transition the fucking image
		VkImageSubresourceRange range;
		range.aspectMask = aspect;
		range.baseArrayLayer = 0;
		range.baseMipLevel = 0;
		range.layerCount = 1;
		range.levelCount = mip_levels;

		VkImageMemoryBarrier img_barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		img_barrier.oldLayout = src_layout;
		img_barrier.newLayout = dst_layout;
		img_barrier.image = img;
		img_barrier.subresourceRange = range;
		img_barrier.srcAccessMask = src_access_mask;
		img_barrier.dstAccessMask = dst_access_mask;

		vkCmdPipelineBarrier(cmd,
			src_stage_mask,
			dst_stage_mask,
			0,
			0, nullptr,
			0, nullptr,
			1, &img_barrier);
	}

	inline void memory_barrier(
		VkCommandBuffer cmd,
		VkAccessFlags src_access, 
		VkAccessFlags dst_access, 
		VkPipelineStageFlags src_stage, 
		VkPipelineStageFlags dst_stage)
	{
		VkMemoryBarrier memory_barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		memory_barrier.srcAccessMask = src_access;
		memory_barrier.dstAccessMask = dst_access;

		
		vkCmdPipelineBarrier(
			cmd,
			src_stage, dst_stage,
			0,
			1, &memory_barrier,
			0, nullptr,
			0, nullptr
		);
	}

	inline u32 aligned_size(u32 size, u32 alignment)
	{
		return (size + alignment - 1) & ~(alignment - 1);
	}

	inline void memory_barrier2(VkCommandBuffer cmd, 
		VkAccessFlags2 src_access_flags, VkAccessFlags2 dst_access_flags,
		VkPipelineStageFlags2 src_stage_flags, VkPipelineStageFlagBits2 dst_stage_flags)
	{
		VkMemoryBarrier2 memory_barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		memory_barrier.srcAccessMask = src_access_flags;
		memory_barrier.srcStageMask = src_stage_flags;
		memory_barrier.dstAccessMask = dst_access_flags;
		memory_barrier.dstStageMask = dst_stage_flags;

		VkDependencyInfo dep_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dep_info.memoryBarrierCount = 1;
		dep_info.pMemoryBarriers = &memory_barrier;
		vkCmdPipelineBarrier2(cmd, &dep_info);
	}

	inline VkImageViewCreateInfo image_view_create_info(VkImage image, VkImageViewType type, VkFormat format, int mip_level = 0)
	{
		VkImageViewCreateInfo cinfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		cinfo.image = image;
		cinfo.viewType = type;
		cinfo.format = format;
		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = mip_level;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;
		cinfo.subresourceRange = range;

		return cinfo;
	}

	inline VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderModule module, VkShaderStageFlagBits stage, const char* entrypoint)
	{
		VkPipelineShaderStageCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		info.stage = stage;
		info.module = module;
		info.pName = entrypoint;

		return info;
	}

	inline VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info(u32 viewport_count, VkViewport* viewports, u32 scissor_count, VkRect2D* scissors, bool dynamic)
	{
		VkPipelineViewportStateCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		
		info.viewportCount = viewport_count;
		info.scissorCount = scissor_count;
		if (!dynamic)
		{
			assert(viewports);
			assert(scissors);
			info.pScissors = scissors;
			info.pViewports = viewports;
		}

		return info;
	}

	inline VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info(VkSampleCountFlagBits samples)
	{
		VkPipelineMultisampleStateCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

		info.rasterizationSamples = samples;
		
		return info;
	}

	inline VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info(u32 attachment_count, VkPipelineColorBlendAttachmentState* attachments)
	{
		VkPipelineColorBlendStateCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		
		info.attachmentCount = attachment_count;
		info.pAttachments = attachments;

		return info;
	}

	inline VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(/*TODO: Fill these params*/)
	{
		VkPipelineColorBlendAttachmentState info{};
		
		info.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		return info;
	}

	inline VkPipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info(u32 dynamic_state_count, VkDynamicState* states)
	{
		VkPipelineDynamicStateCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		info.dynamicStateCount = dynamic_state_count;
		info.pDynamicStates = states;

		return info;
	}

	inline VkPipelineLayoutCreateInfo pipeline_layout_create_info(u32 set_layout_count, VkDescriptorSetLayout* layouts, u32 push_constant_range_count, VkPushConstantRange* push_constant_ranges)
	{
		VkPipelineLayoutCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		
		info.setLayoutCount = set_layout_count;
		info.pSetLayouts = layouts;
		info.pushConstantRangeCount = push_constant_range_count;
		info.pPushConstantRanges = push_constant_ranges;

		return info;
	}

	inline VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info(
		u32 vertex_binding_description_count, VkVertexInputBindingDescription* vertex_binding_descriptions,
		u32 vertex_attribute_description_count, VkVertexInputAttributeDescription* vertex_attribute_descriptions
	)
	{
		VkPipelineVertexInputStateCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		info.vertexBindingDescriptionCount = vertex_binding_description_count;
		info.pVertexBindingDescriptions = vertex_binding_descriptions;
		info.vertexAttributeDescriptionCount = vertex_attribute_description_count;
		info.pVertexAttributeDescriptions = vertex_attribute_descriptions;

		return info;
	}

	inline VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info(VkPrimitiveTopology topology, VkBool32 primitive_restart)
	{
		VkPipelineInputAssemblyStateCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		info.topology = topology;
		info.primitiveRestartEnable = primitive_restart;

		return info;
	}
}