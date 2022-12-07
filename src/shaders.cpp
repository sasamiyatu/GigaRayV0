#include "defines.h"
#include "shaders.h"
#include "common.h"
#include "r_vulkan.h"

bool load_shader_from_file(Shader* shader, VkDevice device, const char* filepath)
{
	memset(shader, 0, sizeof(Shader));
	 // FIXME: This should go through some kind of filesystem
	uint8_t* data;
	uint32_t bytes_read = read_entire_file(filepath, &data);
	assert(bytes_read != 0);
	const size_t spirv_size = bytes_read;
	const uint32_t* spirv_data = (uint32_t*)data;

	VkShaderModuleCreateInfo create_info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = spirv_size;
	create_info.pCode = spirv_data;
	SpvReflectShaderModule module;

	SpvReflectResult result = spvReflectCreateShaderModule((size_t)bytes_read, data, &module);
	assert(result == SPV_REFLECT_RESULT_SUCCESS);

	assert(module.entry_point_count == 1);

	shader->shader_stage = (VkShaderStageFlagBits)module.entry_points[0].shader_stage;
	u32 binding_count = 0;
	spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
	std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
	spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.data());

	u32 pc_count = 0;
	spvReflectEnumeratePushConstantBlocks(&module, &pc_count, nullptr);
	std::vector<SpvReflectBlockVariable*> push_constants(pc_count);
	spvReflectEnumeratePushConstantBlocks(&module, &pc_count, push_constants.data());

	for (const auto& b : bindings)
	{
		if (b->set != 0) continue; // Ignore everything except set 0 for now; set 1 is currently used for bindless descriptors

		shader->descriptor_types[b->binding] = (VkDescriptorType)b->descriptor_type;
		shader->resource_mask |= (1u << b->binding);
	}

	VkShaderModule sm;
	VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &sm));
	shader->shader = sm;

	free(data);
	
	return true;
}
