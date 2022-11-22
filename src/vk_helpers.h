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
	inline static void vk_transition_layout(
			VkCommandBuffer cmd,
			VkImage img,
			VkImageLayout src_layout,
			VkImageLayout dst_layout,
			VkAccessFlags src_access_mask,
			VkAccessFlags dst_access_mask,
			VkPipelineStageFlags src_stage_mask,
			VkPipelineStageFlags dst_stage_mask)
	{
		// Transition the fucking image
		VkImageSubresourceRange range;
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseArrayLayer = 0;
		range.baseMipLevel = 0;
		range.layerCount = 1;
		range.levelCount = 1;

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
}