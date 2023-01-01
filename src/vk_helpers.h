#pragma once
#include "Volk/volk.h"
#include "defines.h"
#include <vector>

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

	inline VkInstanceCreateInfo instance_create_info(const std::vector<const char*> enabled_extensions, const std::vector<const char*> enabled_layers, VkApplicationInfo* app_info)
	{
		VkInstanceCreateInfo info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		info.enabledLayerCount = (u32)enabled_layers.size();
		info.ppEnabledLayerNames = enabled_layers.data();
		info.enabledExtensionCount = (u32)enabled_extensions.size();
		info.ppEnabledExtensionNames = enabled_extensions.data();
		info.pApplicationInfo = app_info;
	}

	inline VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)
	{
		VkCommandBufferBeginInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		info.flags = flags;

		return info;
	}

	inline VkBufferImageCopy buffer_image_copy(VkExtent3D extent)
	{
		VkBufferImageCopy img_copy{};

		VkImageSubresourceLayers subres{};
		subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subres.baseArrayLayer = 0;
		subres.layerCount = 1;
		subres.mipLevel = 0;

		img_copy.bufferOffset = 0;
		img_copy.bufferImageHeight = 0;
		img_copy.bufferRowLength = 0;
		img_copy.imageSubresource = subres;
		img_copy.imageOffset = { 0, 0, 0 };
		img_copy.imageExtent = extent;

		return img_copy;
	}

	inline VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd)
	{
		VkCommandBufferSubmitInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
		info.commandBuffer = cmd;

		return info;
	}

	inline VkSubmitInfo submit_info(u32 command_buffer_count, VkCommandBuffer* cmd, 
		u32 wait_semaphore_count = 0, VkSemaphore* wait_semaphores = 0, 
		u32 signal_semaphore_count = 0, VkSemaphore* signal_semaphores = 0,
		VkPipelineStageFlags* wait_dst_stage = 0)
	{
		VkSubmitInfo info{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		info.waitSemaphoreCount = wait_semaphore_count;
		info.pWaitSemaphores = wait_semaphores;
		info.pWaitDstStageMask = wait_dst_stage;
		info.commandBufferCount = command_buffer_count;
		info.pCommandBuffers = cmd;
		info.signalSemaphoreCount = signal_semaphore_count;
		info.pSignalSemaphores = signal_semaphores;

		return info;
	}

	inline VkBufferCopy buffer_copy(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize src_offset = 0, VkDeviceSize dst_offset = 0)
	{
		VkBufferCopy copy{};
		
		copy.srcOffset = src_offset;
		copy.dstOffset = dst_offset;
		copy.size = size;

		return copy;
	}

	inline VkSamplerCreateInfo sampler_create_info()
	{
		VkSamplerCreateInfo info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

		info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.minFilter = VK_FILTER_LINEAR;
		info.magFilter = VK_FILTER_LINEAR;
		info.maxLod = VK_LOD_CLAMP_NONE;

		return info;
	}

	inline VkRenderingAttachmentInfo rendering_attachment_info(VkImageView view, VkImageLayout layout, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op, VkClearValue clear_value = {})
	{
		VkRenderingAttachmentInfo info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		info.imageView = view;
		info.imageLayout = layout;
		info.loadOp = load_op;
		info.storeOp = store_op;
		info.clearValue = clear_value;
		return info;
	}

	inline VkRenderingInfo rendering_info(
		VkRect2D render_area,
		u32 color_attachment_count,
		VkRenderingAttachmentInfo* color_attachments,
		VkRenderingAttachmentInfo* depth_attachment = nullptr,
		VkRenderingAttachmentInfo* stencil_attachment = nullptr
	)
	{
		VkRenderingInfo info{ VK_STRUCTURE_TYPE_RENDERING_INFO };
		info.renderArea = render_area;
		info.layerCount = 1;
		info.colorAttachmentCount = color_attachment_count;
		info.pColorAttachments = color_attachments;
		info.pDepthAttachment = depth_attachment;
		info.pStencilAttachment = stencil_attachment;

		return info;
	}

	inline VkImageSubresourceLayers image_subresource_layers(VkImageAspectFlags aspect, int mip_level = 0, int base_array_layer = 0, int layer_count = 1)
	{
		VkImageSubresourceLayers layers{};
		layers.aspectMask = aspect;
		layers.mipLevel = mip_level;
		layers.baseArrayLayer = base_array_layer;
		layers.layerCount = layer_count;
		return layers;
	}

	inline VkImageBlit2 image_blit2(VkImageSubresourceLayers src_res, VkOffset3D src_offset0, VkOffset3D src_offset1, VkImageSubresourceLayers dst_res, VkOffset3D dst_offset0, VkOffset3D dst_offset1)
	{
		VkImageBlit2 blit{ VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };
		blit.srcSubresource = src_res;
		blit.srcOffsets[0] = src_offset0;
		blit.srcOffsets[1] = src_offset1;
		blit.dstSubresource = dst_res;
		blit.dstOffsets[0] = dst_offset0;
		blit.dstOffsets[1] = dst_offset1;
		return blit;
	}

	inline VkBlitImageInfo2 blit_image_info2(VkImage src_image, VkImageLayout src_layout, VkImage dst_image, VkImageLayout dst_layout, u32 region_count, VkImageBlit2* regions, VkFilter filter = VK_FILTER_NEAREST)
	{
		VkBlitImageInfo2 info{ VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2 };
		info.srcImage = src_image;
		info.srcImageLayout = src_layout;
		info.dstImage = dst_image;
		info.dstImageLayout = dst_layout;
		info.regionCount = region_count;
		info.pRegions = regions;
		info.filter = filter;
		return info;
	}

	inline VkPresentInfoKHR present_info_khr(u32 wait_semaphore_count, VkSemaphore* wait_semaphores, u32 swapchain_count, VkSwapchainKHR* swapchains, u32* image_indices, VkResult* results = nullptr)
	{
		VkPresentInfoKHR info{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		info.waitSemaphoreCount = wait_semaphore_count;
		info.pWaitSemaphores = wait_semaphores;
		info.swapchainCount = swapchain_count;
		info.pSwapchains = swapchains;
		info.pImageIndices = image_indices;
		info.pResults = results;

		return info;
	}

	inline VkDescriptorSetLayoutBinding descriptor_set_layout_binding(u32 binding, VkDescriptorType type, u32 count, VkShaderStageFlags flags)
	{
		VkDescriptorSetLayoutBinding layout_binding{};
		layout_binding.binding = binding;
		layout_binding.descriptorType = type;
		layout_binding.descriptorCount = count;
		layout_binding.stageFlags = flags;
		return layout_binding;
	}

	inline VkAccelerationStructureGeometryTrianglesDataKHR acceleration_structure_geometry_triangles_data_khr(
		VkFormat vertex_format, 
		VkDeviceAddress vertex_buffer_address,
		VkDeviceSize vertex_stride,
		VkIndexType index_type,
		VkDeviceAddress index_buffer_address,
		u32 max_vertex)
	{
		VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		triangles.vertexFormat = vertex_format;
		triangles.vertexData.deviceAddress = vertex_buffer_address;
		triangles.vertexStride = vertex_stride;
		triangles.indexType = index_type;
		triangles.indexData.deviceAddress = index_buffer_address;
		triangles.maxVertex = max_vertex;
		triangles.transformData = { 0 };
		return triangles;
	}

	inline VkAccelerationStructureGeometryKHR acceleration_structure_geometry_khr(VkAccelerationStructureGeometryTrianglesDataKHR triangles, VkGeometryFlagsKHR flags)
	{
		VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.geometry.triangles = triangles;
		geometry.flags = flags;
		return geometry;
	}

	inline VkAccelerationStructureGeometryKHR acceleration_structure_geometry_khr(VkAccelerationStructureGeometryInstancesDataKHR instances)
	{
		VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		geometry.geometry.instances = instances;

		return geometry;
	}

	inline VkAccelerationStructureBuildRangeInfoKHR acceleration_structure_build_range_info_khr(u32 primitive_count, u32 primitive_offset)
	{
		VkAccelerationStructureBuildRangeInfoKHR range_info{};
		range_info.firstVertex = 0;
		range_info.primitiveCount = primitive_count;
		range_info.primitiveOffset = primitive_offset;
		range_info.transformOffset = 0;
		return range_info;
	}

	inline VkAccelerationStructureBuildGeometryInfoKHR acceleration_structure_build_geometry_info_khr(
		VkBuildAccelerationStructureFlagsKHR flags, 
		u32 geometry_count, VkAccelerationStructureGeometryKHR* geometries,
		VkBuildAccelerationStructureModeKHR mode, VkAccelerationStructureTypeKHR type,
		VkAccelerationStructureKHR src_acceleration_structure = VK_NULL_HANDLE)
	{
		VkAccelerationStructureBuildGeometryInfoKHR build_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		build_info.flags = flags;
		build_info.geometryCount = geometry_count;
		build_info.pGeometries = geometries;
		build_info.mode = mode;
		build_info.type = type;
		build_info.srcAccelerationStructure = src_acceleration_structure;
		return build_info;
	}

	inline VkAccelerationStructureCreateInfoKHR acceleration_structure_create_info(VkAccelerationStructureTypeKHR type, VkDeviceSize size, VkBuffer buffer, VkDeviceSize offset = 0)
	{
		VkAccelerationStructureCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
		create_info.type = type;
		create_info.size = size;
		create_info.buffer = buffer;
		create_info.offset = offset;
		return create_info;
	}

	inline VkAccelerationStructureGeometryInstancesDataKHR acceleration_structure_geometry_instance_data_khr(VkBool32 array_of_pointers, VkDeviceAddress device_address)
	{
		VkAccelerationStructureGeometryInstancesDataKHR instances_vk{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
		instances_vk.arrayOfPointers = VK_FALSE;
		instances_vk.data.deviceAddress = device_address;
		return instances_vk;
	}
}