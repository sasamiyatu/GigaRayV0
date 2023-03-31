#include "sh.h"
#include "shaders.h"

void Probe_System::init(Vk_Context* ctx, VkDescriptorSet bindless_descriptor_set, VkDescriptorSetLayout bindless_set_layout)
{
	this->ctx = ctx;

	constexpr u32 sample_count = 256;
	constexpr u32 required_size = sample_count * sizeof(SH_3);
	probe_samples = ctx->allocate_buffer(required_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

	void* mapped;
	vmaMapMemory(ctx->allocator, probe_samples.allocation, &mapped);
	memset(mapped, 0, required_size);
	mapped_probe_data = (SH_3*)mapped;
	vmaUnmapMemory(ctx->allocator, probe_samples.allocation);

	debug_mesh = create_sphere(16);

	this->bindless_descriptor_set = bindless_descriptor_set;
	sh_integrate_pipeline = ctx->create_compute_pipeline("shaders/spirv/integrate_sh.comp.spv", bindless_set_layout);
}

void Probe_System::init_probe_grid(glm::vec3 min, glm::vec3 max)
{
	bbmin = min - glm::mod(min, glm::vec3(probe_spacing));
	bbmax = max + (probe_spacing - glm::mod(max, glm::vec3(probe_spacing)));

	glm::uvec3 probe_counts = glm::uvec3((bbmax - bbmin) / probe_spacing + 1.0f);

	int a = 5;
}

void Probe_System::bake(VkCommandBuffer cmd, Cubemap* envmap, VkSampler sampler)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sh_integrate_pipeline.pipeline);

	Descriptor_Info descriptor_info[] =
	{
		Descriptor_Info(probe_samples.buffer, 0, VK_WHOLE_SIZE),
		Descriptor_Info(sampler, envmap->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
	};

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, sh_integrate_pipeline.layout, 1, 1, &bindless_descriptor_set, 0, nullptr);
	vkCmdPushDescriptorSetWithTemplateKHR(cmd, sh_integrate_pipeline.update_template, sh_integrate_pipeline.layout, 0, descriptor_info);
	
	vkCmdDispatch(cmd, 1, 1, 1);

	create_vertex_buffer(&debug_mesh, cmd, ctx);
	create_index_buffer(&debug_mesh, cmd, ctx);
}
