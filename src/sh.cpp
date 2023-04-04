#include "sh.h"
#include "shaders.h"
#include "scene.h"
#include "logging.h"

void Probe_System::init(Vk_Context* ctx, VkDescriptorSet bindless_descriptor_set, VkDescriptorSetLayout bindless_set_layout, GPU_Buffer* gpu_camera_data, Scene* scene, VkCommandBuffer cmd)
{
	this->ctx = ctx;
	this->gpu_camera_data = gpu_camera_data;
	this->scene = scene;

	debug_mesh = create_sphere(4);

	this->bindless_descriptor_set = bindless_descriptor_set;
	sh_integrate_pipeline = ctx->create_compute_pipeline("shaders/spirv/integrate_sh.comp.spv", bindless_set_layout);
	sh_debug_rendering_pipeline = ctx->create_raster_pipeline("shaders/spirv/sh_debug.vert.spv", "shaders/spirv/sh_debug.frag.spv");

	create_vertex_buffer(&debug_mesh, cmd, ctx);
	create_index_buffer(&debug_mesh, cmd, ctx);
}

void Probe_System::init_probe_grid(glm::vec3 min, glm::vec3 max)
{
	bbmin = min - glm::mod(min, glm::vec3(probe_spacing));
	bbmax = max + (probe_spacing - glm::mod(max, glm::vec3(probe_spacing)));

	probe_counts = glm::uvec3((bbmax - bbmin) / probe_spacing + 1.0f);

	if (glm::any(glm::isnan(bbmin)) || glm::any(glm::isnan(bbmax)))
		probe_counts = glm::uvec3(1);

	const u32 total_probe_count = probe_counts.x * probe_counts.y * probe_counts.z;
	const u32 required_size = total_probe_count * sizeof(SH_3);
	probe_samples = ctx->allocate_buffer(required_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
	void* mapped;
	vmaMapMemory(ctx->allocator, probe_samples.allocation, &mapped);
	mapped_probe_data = (SH_3*)mapped;
	for (u32 i = 0; i < total_probe_count; ++i)
		for (int j = 0; j < 9; ++j)
			mapped_probe_data[i].coefs[j] = glm::vec3(0.0f);
	vmaUnmapMemory(ctx->allocator, probe_samples.allocation);
}

void Probe_System::bake(VkCommandBuffer cmd, Cubemap* envmap, VkSampler sampler)
{
	assert(scene->tlas.has_value());

	if (samples_accumulated >= max_samples) return;
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sh_integrate_pipeline.pipeline);

	Descriptor_Info descriptor_info[] =
	{
		Descriptor_Info(probe_samples.buffer, 0, VK_WHOLE_SIZE),
		Descriptor_Info(sampler, envmap->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		Descriptor_Info(scene->tlas.value().acceleration_structure)
	};

	struct
	{
		glm::uvec3 probe_counts;
		float probe_spacing;
		glm::vec3 probe_min;
		u32 samples_accumulated;
	} pc;

	pc.probe_counts = probe_counts;
	pc.probe_spacing = probe_spacing;
	pc.probe_min = bbmin;
	pc.samples_accumulated = samples_accumulated;
	vkCmdPushConstants(cmd, sh_integrate_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sh_integrate_pipeline.layout, 1, 1, &bindless_descriptor_set, 0, nullptr);
	vkCmdPushDescriptorSetWithTemplateKHR(cmd, sh_integrate_pipeline.update_template, sh_integrate_pipeline.layout, 0, descriptor_info);
	
	vkCmdDispatch(cmd, probe_counts.x, probe_counts.y, probe_counts.z);

	samples_accumulated += samples_per_pass;
	if (samples_accumulated >= max_samples)
	{
		LOG_DEBUG("Probes have accumulated %d samples, ending bake\n", max_samples);
	}
}

// Assumes a render pass has been started outside this function
void Probe_System::debug_render(VkCommandBuffer cmd)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sh_debug_rendering_pipeline.pipeline);

	Descriptor_Info descriptor_info[] =
	{
		Descriptor_Info(debug_mesh.vertex_buffer.buffer, 0, VK_WHOLE_SIZE),
		Descriptor_Info(debug_mesh.index_buffer.buffer, 0, VK_WHOLE_SIZE),
		Descriptor_Info(gpu_camera_data->gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
		Descriptor_Info(probe_samples.buffer, 0, VK_WHOLE_SIZE),
	};

	struct
	{
		glm::uvec3 probe_counts;
		float probe_spacing;
		glm::vec3 probe_min;
	} pc;

	pc.probe_counts = probe_counts;
	pc.probe_spacing = probe_spacing;
	pc.probe_min = bbmin;
	vkCmdPushConstants(cmd, sh_debug_rendering_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
	vkCmdBindIndexBuffer(cmd, debug_mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdPushDescriptorSetWithTemplateKHR(cmd, sh_debug_rendering_pipeline.update_template, sh_debug_rendering_pipeline.layout, 0, descriptor_info);

	u32 probe_count = probe_counts.x * probe_counts.y * probe_counts.z;
	vkCmdDrawIndexed(cmd, (u32)debug_mesh.indices.size(), probe_count, 0, 0, 0);
}

void Probe_System::shutdown()
{
	vkDestroyPipelineLayout(ctx->device, sh_debug_rendering_pipeline.layout, nullptr);
	vkDestroyPipeline(ctx->device, sh_debug_rendering_pipeline.pipeline, nullptr);
	vkDestroyDescriptorSetLayout(ctx->device, sh_debug_rendering_pipeline.desc_sets[0], nullptr);
	vkDestroyDescriptorUpdateTemplate(ctx->device, sh_debug_rendering_pipeline.update_template, nullptr);
}
