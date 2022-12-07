#pragma once

struct Shader
{
	VkShaderModule shader;
	VkShaderStageFlagBits shader_stage;
	
	VkDescriptorType descriptor_types[32]; // Max 32 push descriptors
	u32 resource_mask; // Which resources are actually in use
};

struct Descriptor_Acceleration_Structure_Info
{
	VkAccelerationStructureKHR acceleration_structure;
};

struct Descriptor_Info
{
	union
	{
		VkDescriptorBufferInfo buffer_info;
		VkDescriptorImageInfo image_info;
		Descriptor_Acceleration_Structure_Info acceleration_structure_info;
	};

	Descriptor_Info()
	{
	}

	Descriptor_Info(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
	{
		buffer_info.buffer = buffer;
		buffer_info.offset = offset;
		buffer_info.range = range;
	}

	Descriptor_Info(VkSampler sampler, VkImageView image_view, VkImageLayout image_layout)
	{
		image_info.sampler = sampler;
		image_info.imageView = image_view;
		image_info.imageLayout = image_layout;
	}

	Descriptor_Info(VkBuffer buffer)
	{
		buffer_info.buffer = buffer;
		buffer_info.offset = 0;
		buffer_info.range = VK_WHOLE_SIZE;
	}

	Descriptor_Info(VkAccelerationStructureKHR acceleration_structure)
	{
		acceleration_structure_info.acceleration_structure = acceleration_structure;
	}
};

bool load_shader_from_file(Shader* shader, VkDevice device, const char* filepath);