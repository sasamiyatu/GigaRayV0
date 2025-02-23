#include "renderer.h"
#include "common.h"
#include "vk_helpers.h"
#include "shaders.h"

#include <assert.h>
#include <array>
#include <r_mesh.h>
#include <ecs.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"

#include "settings.h"

using namespace vkinit;

static constexpr int BINDLESS_TEXTURE_BINDING = 0;
static constexpr int BINDLESS_VERTEX_BINDING = 1;
static constexpr int BINDLESS_INDEX_BINDING = 2;
static constexpr int BINDLESS_UNIFORM_BINDING = 3;
static constexpr int BINDLESS_INSTANCE_DATA_BINDING = 4;
static constexpr int MAX_MATERIAL_COUNT = 256;

static void vk_begin_command_buffer(VkCommandBuffer cmd);

constexpr VkFormat NORMAL_ROUGHNESS_FORMAT = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
constexpr VkFormat BASECOLOR_METALNESS_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

constexpr u32 MAX_INDIRECT_DRAWS = 1'000'000;
constexpr int MAX_BINDLESS_RESOURCES = 16536;
constexpr int TAA_SAMPLE_COUNT = 8;

#define PING 0
#define PONG 1

constexpr float van_der_corput_base_2[64] = {
	0.000000f,
	0.500000f,
	0.250000f,
	0.750000f,
	0.125000f,
	0.625000f,
	0.375000f,
	0.875000f,
	0.062500f,
	0.562500f,
	0.312500f,
	0.812500f,
	0.187500f,
	0.687500f,
	0.437500f,
	0.937500f,
	0.031250f,
	0.531250f,
	0.281250f,
	0.781250f,
	0.156250f,
	0.656250f,
	0.406250f,
	0.906250f,
	0.093750f,
	0.593750f,
	0.343750f,
	0.843750f,
	0.218750f,
	0.718750f,
	0.468750f,
	0.968750f,
	0.015625f,
	0.515625f,
	0.265625f,
	0.765625f,
	0.140625f,
	0.640625f,
	0.390625f,
	0.890625f,
	0.078125f,
	0.578125f,
	0.328125f,
	0.828125f,
	0.203125f,
	0.703125f,
	0.453125f,
	0.953125f,
	0.046875f,
	0.546875f,
	0.296875f,
	0.796875f,
	0.171875f,
	0.671875f,
	0.421875f,
	0.921875f,
	0.109375f,
	0.609375f,
	0.359375f,
	0.859375f,
	0.234375f,
	0.734375f,
	0.484375f,
	0.984375f
};

constexpr vec2 halton_sequence[] = {
	vec2(0.5, 0.3333333333333333),
	vec2(0.25, 0.6666666666666666),
	vec2(0.75, 0.1111111111111111),
	vec2(0.125, 0.4444444444444444),
	vec2(0.625, 0.7777777777777778),
	vec2(0.375, 0.2222222222222222),
	vec2(0.875, 0.5555555555555556),
	vec2(0.0625, 0.8888888888888888),
	vec2(0.5625, 0.037037037037037035),
	vec2(0.3125, 0.37037037037037035),
	vec2(0.8125, 0.7037037037037037),
	vec2(0.1875, 0.14814814814814814),
	vec2(0.6875, 0.48148148148148145),
	vec2(0.4375, 0.8148148148148148),
	vec2(0.9375, 0.25925925925925924),
	vec2(0.03125, 0.5925925925925926),
	vec2(0.53125, 0.9259259259259259),
	vec2(0.28125, 0.07407407407407407),
	vec2(0.78125, 0.4074074074074074),
	vec2(0.15625, 0.7407407407407407),
	vec2(0.65625, 0.18518518518518517),
	vec2(0.40625, 0.5185185185185185),
	vec2(0.90625, 0.8518518518518519),
	vec2(0.09375, 0.2962962962962963),
	vec2(0.59375, 0.6296296296296297),
	vec2(0.34375, 0.9629629629629629),
	vec2(0.84375, 0.012345679012345678),
	vec2(0.21875, 0.345679012345679),
	vec2(0.71875, 0.6790123456790124),
	vec2(0.46875, 0.12345679012345678),
	vec2(0.96875, 0.4567901234567901),
	vec2(0.015625, 0.7901234567901234),
	vec2(0.515625, 0.2345679012345679),
	vec2(0.265625, 0.5679012345679012),
	vec2(0.765625, 0.9012345679012346),
	vec2(0.140625, 0.04938271604938271),
	vec2(0.640625, 0.38271604938271603),
	vec2(0.390625, 0.7160493827160493),
	vec2(0.890625, 0.16049382716049382),
	vec2(0.078125, 0.49382716049382713),
	vec2(0.578125, 0.8271604938271605),
	vec2(0.328125, 0.2716049382716049),
	vec2(0.828125, 0.6049382716049383),
	vec2(0.203125, 0.9382716049382716),
	vec2(0.703125, 0.08641975308641975),
	vec2(0.453125, 0.41975308641975306),
	vec2(0.953125, 0.7530864197530864),
	vec2(0.046875, 0.19753086419753085),
	vec2(0.546875, 0.5308641975308642),
	vec2(0.296875, 0.8641975308641975),
	vec2(0.796875, 0.30864197530864196),
	vec2(0.171875, 0.6419753086419753),
	vec2(0.671875, 0.9753086419753086),
	vec2(0.421875, 0.024691358024691357),
	vec2(0.921875, 0.35802469135802467),
	vec2(0.109375, 0.691358024691358),
	vec2(0.609375, 0.13580246913580246),
	vec2(0.359375, 0.4691358024691358),
	vec2(0.859375, 0.8024691358024691),
	vec2(0.234375, 0.24691358024691357),
	vec2(0.734375, 0.5802469135802469),
	vec2(0.484375, 0.9135802469135802),
	vec2(0.984375, 0.06172839506172839),
	vec2(0.0078125, 0.3950617283950617),
};

// FIXME: We're gonna need more stuff than this 
void Renderer::create_bindless_descriptor_set_layout()
{
	VkShaderStageFlags flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
	// Bindless stuff

	VkDescriptorSetLayoutBinding bindless_bindings[] = {
	vkinit::descriptor_set_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_BINDLESS_RESOURCES, flags),
	vkinit::descriptor_set_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BINDLESS_RESOURCES, flags),
	vkinit::descriptor_set_layout_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BINDLESS_RESOURCES, flags),
	vkinit::descriptor_set_layout_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, flags),
	vkinit::descriptor_set_layout_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BINDLESS_RESOURCES, flags),
	};

	VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_info.bindingCount = uint32_t(std::size(bindless_bindings));
	layout_info.pBindings = bindless_bindings;
	layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

	VkDescriptorBindingFlags bindless_flags = 
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT 
		| VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;

	VkDescriptorBindingFlags flags2[5] = { bindless_flags, bindless_flags, bindless_flags, 0, bindless_flags };

	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT, nullptr };
	extended_info.bindingCount = (u32)std::size(bindless_bindings);
	extended_info.pBindingFlags = flags2;
	layout_info.pNext = &extended_info;

	VK_CHECK(vkCreateDescriptorSetLayout(context->device, &layout_info, nullptr, &bindless_set_layout));

	g_garbage_collector->push([=]()
		{
			vkDestroyDescriptorSetLayout(context->device, bindless_set_layout, nullptr);
		}, Garbage_Collector::SHUTDOWN);


	// Allocate bindless descriptors
	
	VkDescriptorSetAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &bindless_set_layout;

	VK_CHECK(vkAllocateDescriptorSets(context->device, &alloc_info, &bindless_descriptor_set));
	// NOTE: No descriptor pools because we are using push descriptors for now!!
}

VkDescriptorSetLayout Renderer::create_descriptor_set_layout(Shader* shader)
{
	VkDescriptorSetLayoutCreateInfo cinfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	assert(shader->resource_mask != 0);
	u32 binding_count = 0;
	for (int n = shader->resource_mask; n != 0; n >>= 1)
		binding_count++;

	std::vector<VkDescriptorSetLayoutBinding> bindings(binding_count);

	for (u32 i = 0; i < binding_count; ++i)
	{
		if ((shader->resource_mask >> i) & 1)
		{
			bindings[i].binding = i;
			bindings[i].descriptorCount = 1;
			bindings[i].descriptorType = shader->descriptor_types[i];
			bindings[i].stageFlags = shader->shader_stage;
		}
	}

	cinfo.bindingCount = binding_count;
	cinfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	cinfo.pBindings = bindings.data();

	VkDescriptorSetLayout layout;
	vkCreateDescriptorSetLayout(context->device, &cinfo, nullptr, &layout);
	return layout;
}


// FIXME: Hardcoded shaders and hitgroups
Vk_Pipeline Renderer::vk_create_rt_pipeline()
{
	Shader rgen_shader, rmiss_shader, rchit_shader;
	load_shader_from_file(&rgen_shader, context->device, "shaders/spirv/test.rgen.spv");
	load_shader_from_file(&rmiss_shader, context->device, "shaders/spirv/test.rmiss.spv");
	load_shader_from_file(&rchit_shader, context->device, "shaders/spirv/test.rchit.spv");
	
	VkDescriptorSetLayout push_desc_layout = create_descriptor_set_layout(&rgen_shader);
	VkDescriptorSetLayout layouts[] = { push_desc_layout, bindless_set_layout };
	Raytracing_Pipeline rt_pp = context->create_raytracing_pipeline(rgen_shader.shader, rmiss_shader.shader, rchit_shader.shader, layouts, (int)std::size(layouts));

	shader_binding_table = rt_pp.shader_binding_table.sbt_data;

	vkDestroyShaderModule(context->device, rgen_shader.shader, nullptr);
	vkDestroyShaderModule(context->device, rmiss_shader.shader, nullptr);
	vkDestroyShaderModule(context->device, rchit_shader.shader, nullptr);

	descriptor_update_template = context->create_descriptor_update_template(1, &rgen_shader, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pp.pipeline.layout);

	return rt_pp.pipeline;
}

Raytracing_Pipeline Renderer::create_gbuffer_rt_pipeline()
{
	constexpr char* rgen_src = "shaders/spirv/primary_ray.rgen.spv";
	constexpr char* rmiss_src = "shaders/spirv/primary_ray.rmiss.spv";
	constexpr char* rchit_src = "shaders/spirv/primary_ray.rchit.spv";

	VkShaderModule rgen_shader, rmiss_shader, rchit_shader;
	VkDescriptorSetLayout layout;
	{
		u8* data;
		u32 size = read_entire_file(rgen_src, &data);

		SpvReflectShaderModule module;
		SpvReflectResult result = spvReflectCreateShaderModule((size_t)size, data, &module);
		assert(result == SPV_REFLECT_RESULT_SUCCESS);

		u32 binding_count = 0;
		spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
		std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
		spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.data());

		u32 pc_count = 0;
		spvReflectEnumeratePushConstantBlocks(&module, &pc_count, nullptr);
		std::vector<SpvReflectBlockVariable*> push_constants(pc_count);
		spvReflectEnumeratePushConstantBlocks(&module, &pc_count, push_constants.data());

		rgen_shader = context->create_shader_module_from_bytes((u32*)data, size);

		// create descriptor set layout for render targets
		std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
		for (const auto& b : bindings)
		{
			if (b->set == 0)
			{
				VkDescriptorSetLayoutBinding bind{};
				bind.binding = b->binding;
				bind.descriptorCount = b->count;
				bind.descriptorType = (VkDescriptorType)b->descriptor_type;
				bind.pImmutableSamplers = nullptr;
				bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
				layout_bindings.push_back(bind);
			}
		}

		VkDescriptorSetLayoutCreateInfo cinfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		cinfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		cinfo.bindingCount = (u32)layout_bindings.size();
		cinfo.pBindings = layout_bindings.data();

		vkCreateDescriptorSetLayout(context->device, &cinfo, nullptr, &layout);

		rmiss_shader = context->create_shader_module_from_file(rmiss_src);
		rchit_shader = context->create_shader_module_from_file(rchit_src);

		free(data);
	}

	std::array<VkDescriptorSetLayout, 2> layouts = { layout, bindless_set_layout };
	Raytracing_Pipeline rt_pp = context->create_raytracing_pipeline(
		rgen_shader, rmiss_shader, rchit_shader,
		layouts.data(), (int)layouts.size()
	);

	g_garbage_collector->push([=]()
		{
			vkDestroyPipeline(context->device, rt_pp.pipeline.pipeline, nullptr);
			vkDestroyPipelineLayout(context->device, rt_pp.pipeline.layout, nullptr);
			vkDestroyDescriptorSetLayout(context->device, layout, nullptr);
		}, Garbage_Collector::SHUTDOWN);

	vkDestroyShaderModule(context->device, rgen_shader, nullptr);
	vkDestroyShaderModule(context->device, rmiss_shader, nullptr);
	vkDestroyShaderModule(context->device, rchit_shader, nullptr);

	return rt_pp;
}

void Renderer::create_samplers()
{
	VkSamplerCreateInfo cinfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	cinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	cinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	cinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	cinfo.minFilter = VK_FILTER_LINEAR;
	cinfo.magFilter = VK_FILTER_LINEAR;
	cinfo.maxLod = VK_LOD_CLAMP_NONE;
	cinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	vkCreateSampler(context->device, &cinfo, nullptr, &samplers[BILINEAR_SAMPLER_WRAP]);
	cinfo.addressModeU = cinfo.addressModeV = cinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	vkCreateSampler(context->device, &cinfo, nullptr, &samplers[BILINEAR_SAMPLER_CLAMP]);

	cinfo.minFilter = VK_FILTER_NEAREST;
	cinfo.magFilter = VK_FILTER_NEAREST;
	vkCreateSampler(context->device, &cinfo, nullptr, &samplers[POINT_SAMPLER]);
}


Renderer::Renderer(Vk_Context* context, Platform* platform,
	Resource_Manager<Mesh>* mesh_manager,
	Resource_Manager<Texture>* texture_manager,
	Resource_Manager<Material>* material_manager,
	Timer* timer)
	: context(context),
	platform(platform),
	mesh_manager(mesh_manager),
	texture_manager(texture_manager),
	material_manager(material_manager),
	timer(timer)
{
	initialize();
}

void Renderer::create_cubemap_from_envmap()
{
	VkCommandBuffer cmd = get_current_frame_command_buffer();
	vk_begin_command_buffer(cmd);
	vk_transition_layout(cmd, cubemap.image.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_ACCESS_NONE, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	VkClearValue colors[6] = {
		{127.0f / 255.0f, 255.0f / 255.0f, 212.0f / 255.0f},
		{200.0f / 255.0f, 162.0f / 255.0f, 200.0f / 255.0f},
		{255.0f / 255.0f, 229.0f / 255.0f, 180.0f / 255.0f},
		{ 65.0f / 255.0f, 102.0f / 255.0f, 245.0f / 255.0f},
		{255.0f / 255.0f, 255.0f / 255.0f,   0.0f / 255.0f},
		{128.0f / 255.0f,   0.0f / 255.0f,  32.0f / 255.0f}
	};

	glm::mat4 view_matrices[6] = {
		glm::lookAt(glm::vec3(0.0), glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0)),
		glm::lookAt(glm::vec3(0.0), glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0)),
		glm::lookAt(glm::vec3(0.0), glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)),
		glm::lookAt(glm::vec3(0.0), glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)),
		glm::lookAt(glm::vec3(0.0), glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, 1.0, 0.0)),
		glm::lookAt(glm::vec3(0.0), glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, 1.0, 0.0))
	};

	glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

	for (u32 face = 0; face < 6; ++face)
	{
		VkRenderingAttachmentInfo attachment_info = vkinit::rendering_attachment_info(cubemap.image_views[face], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, colors[face]);
		VkRect2D render_area = { {0, 0}, {cubemap.size, cubemap.size} };
		VkRenderingInfo rendering_info = vkinit::rendering_info(render_area, 1, &attachment_info);
		vkCmdBeginRendering(cmd, &rendering_info);

		VkViewport viewport{ 0.0f, (float)cubemap.size, (float)cubemap.size, -(float)cubemap.size, 0.0f, 1.0f };


		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &render_area);

		//struct {
		//	glm::uvec2 size;
		//	u32 face_index;
		//} pc;
		//pc.size = glm::uvec2(cubemap.size);
		//pc.face_index = face;

		struct {
			glm::mat4 viewproj;
		} pc;
		pc.viewproj = proj * view_matrices[face];

		Descriptor_Info descriptor_info[] =
		{
			Descriptor_Info(samplers[BILINEAR_SAMPLER_WRAP], environment_map.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		};

		vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[GENERATE_CUBEMAP_PIPELINE2].update_template, pipelines[GENERATE_CUBEMAP_PIPELINE2].layout, 0, descriptor_info);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[GENERATE_CUBEMAP_PIPELINE2].pipeline);
		vkCmdPushConstants(cmd, pipelines[GENERATE_CUBEMAP_PIPELINE2].layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		//vkCmdDraw(cmd, 6, 1, 0, 0);
		vkCmdDraw(cmd, 36, 1, 0, 0);

		vkCmdEndRendering(cmd);
	}

	vk_transition_layout(cmd, cubemap.image.image,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
	);

	vkEndCommandBuffer(cmd);
	vk_command_buffer_single_submit(cmd);
	vkQueueWaitIdle(context->graphics_queue);
}
void Renderer::initialize()
{
	math::pcg32_srandom_r(&rng_state, 1337, 42);

	create_descriptor_pools();
	create_bindless_descriptor_set_layout();

	create_lookup_textures();

	std::string blue_noise_tex_base = "data/bluenoise/stbn_unitvec3_cosine_2Dx1D_128x128x64_";
	for (int i = 0; i < BLUE_NOISE_TEXTURE_COUNT; ++i)
	{
		std::string path = blue_noise_tex_base + std::to_string(i) + ".png";
		blue_noise[i] = context->load_texture(path.c_str());
	}

	std::string blue_noise_tex_base2 = "data/bluenoise/stbn_scalar_2Dx1Dx1D_128x128x64x1_";
	for (int i = 0; i < BLUE_NOISE_TEXTURE_COUNT; ++i)
	{
		std::string path = blue_noise_tex_base2 + std::to_string(i) + ".png";
		blue_noise_scalar[i] = context->load_texture(path.c_str());
	}

	std::string blue_noise_tex_base3 = "data/bluenoise/stbn_vec2_2Dx1D_128x128x64_";
	for (int i = 0; i < BLUE_NOISE_TEXTURE_COUNT; ++i)
	{
		std::string path = blue_noise_tex_base3 + std::to_string(i) + ".png";
		blue_noise_vec2[i] = context->load_texture(path.c_str());
	}

	pipelines[PATH_TRACER_PIPELINE] = vk_create_rt_pipeline();
	//primary_ray_pipeline = create_gbuffer_rt_pipeline();
	{
		Raster_Options opt;
		opt.color_attachment_count = 4;
		opt.color_formats[0] = VK_FORMAT_R32G32B32A32_SFLOAT;
		opt.color_formats[1] = NORMAL_ROUGHNESS_FORMAT;
		opt.color_formats[2] = BASECOLOR_METALNESS_FORMAT;
		opt.color_formats[3] = VK_FORMAT_R32G32B32A32_SFLOAT;
		//opt.cull_mode = VK_CULL_MODE_NONE;
		pipelines[RASTER_PIPELINE] = create_raster_graphics_pipeline("shaders/spirv/basic.vert.spv", "shaders/spirv/basic.frag.spv", true, opt);

		opt.depth_write_enable = VK_FALSE;
		pipelines[SKYBOX_PIPELINE] = create_raster_graphics_pipeline("shaders/spirv/skybox.vert.spv", "shaders/spirv/skybox.frag.spv", false, opt);
	}
	pipelines[CUBEMAP_PIPELINE] = create_raster_graphics_pipeline("shaders/spirv/cube_test.vert.spv", "shaders/spirv/cube_test.frag.spv", true);

	Raster_Options opt;
	opt.color_formats[0] = VK_FORMAT_R16G16B16A16_SFLOAT;
	//pipelines[GENERATE_CUBEMAP_PIPELINE] = create_raster_graphics_pipeline("shaders/spirv/fullscreen_quad.vert.spv", "shaders/spirv/equirectangular_to_cubemap.frag.spv", false, opt);
	pipelines[GENERATE_CUBEMAP_PIPELINE2] = create_raster_graphics_pipeline("shaders/spirv/generate_cubemap.vert.spv", "shaders/spirv/equirectangular_to_cubemap.frag.spv", false, opt);
	pipelines[INDIRECT_DIFFUSE_PIPELINE] = context->create_compute_pipeline("shaders/spirv/indirect_diffuse.comp.spv", bindless_set_layout);
	pipelines[INDIRECT_SPECULAR_PIPELINE] = context->create_compute_pipeline("shaders/spirv/indirect_specular.comp.spv", bindless_set_layout);

	cubemap = context->create_cubemap(512, VK_FORMAT_R16G16B16A16_SFLOAT);

	u32 indirect_buffer_size = MAX_INDIRECT_DRAWS * sizeof(VkDrawIndexedIndirectCommand);
	indirect_draw_buffer = context->create_gpu_buffer(indirect_buffer_size, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	transition_swapchain_images(get_current_frame_command_buffer());
	create_render_targets(get_current_frame_command_buffer());


	pipelines[COMPOSITION_PIPELINE] = context->create_compute_pipeline("shaders/spirv/composition.comp.spv");
	pipelines[TEMPORAL_ACCUMULATION] = context->create_compute_pipeline("shaders/spirv/temporal_accumulation.comp.spv");
	pipelines[HISTORY_FIX_MIP_GEN] = context->create_compute_pipeline("shaders/spirv/history_fix_mip_gen.comp.spv");
	pipelines[HISTORY_FIX] = context->create_compute_pipeline("shaders/spirv/history_fix.comp.spv");
	pipelines[HISTORY_FIX_ALTERNATIVE] = context->create_compute_pipeline("shaders/spirv/history_fix_alternative.comp.spv");
	pipelines[TONEMAP_AND_TAA] = context->create_compute_pipeline("shaders/spirv/tonemap_and_taa.comp.spv");

	VkSpecializationMapEntry map_entries[] = {
		{0, 0, sizeof(int)},
		{1, sizeof(int), sizeof(int)}
	};
	VkSpecializationInfo spec_info = {};
	int spec_data[] = { PRE_BLUR_CONSTANT_ID, BLUR_CHANNEL_DIFFUSE };
	spec_info.dataSize = sizeof(spec_data);
	spec_info.mapEntryCount = (u32)std::size(map_entries);
	spec_info.pData = &spec_data;
	spec_info.pMapEntries = map_entries;
	pipelines[PRE_BLUR] = context->create_compute_pipeline("shaders/spirv/blur.comp.spv", VK_NULL_HANDLE, &spec_info);

	spec_data[1] = BLUR_CHANNEL_SPECULAR;
	pipelines[PRE_BLUR_SPEC] = context->create_compute_pipeline("shaders/spirv/blur.comp.spv", VK_NULL_HANDLE, &spec_info);
	
	spec_data[0] = BLUR_CONSTANT_ID;
	spec_data[1] = BLUR_CHANNEL_DIFFUSE;
	pipelines[BLUR] = context->create_compute_pipeline("shaders/spirv/blur.comp.spv", VK_NULL_HANDLE, &spec_info);
	
	spec_data[1] = BLUR_CHANNEL_SPECULAR;
	pipelines[BLUR_SPEC] = context->create_compute_pipeline("shaders/spirv/blur.comp.spv", VK_NULL_HANDLE, &spec_info);
	
	spec_data[0] = POST_BLUR_CONSTANT_ID;
	spec_data[1] = BLUR_CHANNEL_DIFFUSE;
	pipelines[POST_BLUR] = context->create_compute_pipeline("shaders/spirv/blur.comp.spv", VK_NULL_HANDLE, &spec_info);

	spec_data[1] = BLUR_CHANNEL_SPECULAR;
	pipelines[POST_BLUR_SPEC] = context->create_compute_pipeline("shaders/spirv/blur.comp.spv", VK_NULL_HANDLE, &spec_info);
	
	pipelines[TEMPORAL_STABILIZATION] = context->create_compute_pipeline("shaders/spirv/temporal_stabilization.comp.spv");
	
	create_samplers();

	// TODO: Create separate buffer for each in flight frame when we remove vkDeviceWaitIdle / vkQueueWaitIdle from the main loop
	global_constants_buffer = context->allocate_buffer(sizeof(Global_Constants_Data), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	vmaMapMemory(context->allocator, global_constants_buffer.allocation, (void**)&global_constants_data);

	for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
		query_pools[i] = context->create_query_pool();


	{ // Reset query pools
		VkCommandBuffer cmd = get_current_frame_command_buffer();
		vk_begin_command_buffer(cmd);
		for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
			vkCmdResetQueryPool(cmd, query_pools[i], 0, 128);
		vkEndCommandBuffer(cmd);
		vk_command_buffer_single_submit(cmd);
		vkQueueWaitIdle(context->graphics_queue);
	}

	platform->get_window_size(&window_width, &window_height);
	aspect_ratio = (float)window_width / (float)window_height;
	initialized = true;

	ui_overlay.initialize(context, window_width, window_height, &final_output, platform->window.window);
}

VkCommandBuffer Renderer::get_current_frame_command_buffer()
{
	return context->frame_objects[current_frame_index].cmd;
}

void Renderer::create_descriptor_pools()
{
	VkDescriptorPoolCreateInfo cinfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	VkDescriptorPoolSize pool_sizes_bindless[] =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_BINDLESS_RESOURCES },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BINDLESS_RESOURCES}
	};

	cinfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
	cinfo.maxSets = (u32)(std::size(pool_sizes_bindless) * MAX_BINDLESS_RESOURCES);
	cinfo.poolSizeCount = (u32)std::size(pool_sizes_bindless);
	cinfo.pPoolSizes = pool_sizes_bindless;

	vkCreateDescriptorPool(context->device, &cinfo, nullptr, &descriptor_pool);
	g_garbage_collector->push([=]()
		{
			vkDestroyDescriptorPool(context->device, descriptor_pool, nullptr);
		}, Garbage_Collector::SHUTDOWN);
}

void Renderer::vk_command_buffer_single_submit(VkCommandBuffer cmd)
{
	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd;

	vkQueueSubmit(context->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
}

void Renderer::do_frame(ECS* ecs, float dt)
{
	pre_frame();
	begin_frame();
	draw(ecs, dt);
	end_frame();
}

// Loads the meshes from the scene and creates the acceleration structures
void Renderer::init_scene(ECS* ecs)
{
	constexpr char* envmap_src = "data/envmaps/piazza_bologni_4k.hdr";
	//char* envmap_src = "data/golf_course_sunrise_4k.hdr";
	//constexpr char* envmap_src = "data/kloppenheim_06_puresky_4k.hdr";
	environment_map = context->load_texture_hdri(
		envmap_src,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	create_cubemap_from_envmap();
	
	{
		// Update bindless textures
		size_t resource_count = (u32)texture_manager->resources.size();
		std::vector<VkWriteDescriptorSet> writes(resource_count);
		std::vector<VkDescriptorImageInfo> img_infos(resource_count);
		std::vector<VkDescriptorBufferInfo> buf_infos;
		for (size_t i = 0; i < resource_count; ++i)
		{
			VkWriteDescriptorSet& w = writes[i];
			w = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			w.descriptorCount = 1;
			w.dstArrayElement = (u32)i;
			w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			w.dstSet = bindless_descriptor_set;
			w.dstBinding = BINDLESS_TEXTURE_BINDING;
			
			VkDescriptorImageInfo& img = img_infos[i];
			img.sampler = samplers[BILINEAR_SAMPLER_WRAP];
			img.imageView = texture_manager->resources[i].resource.image.image_view;
			img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			w.pImageInfo = &img;
		}
		
		{
			// Update material descriptions
			size_t material_count = (u32)material_manager->resources.size();
			scene.material_buffer = context->allocate_buffer(sizeof(Material) * MAX_MATERIAL_COUNT,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

			void* mapped;
			vmaMapMemory(context->allocator, scene.material_buffer.allocation, &mapped);
			Material* writeaddr = (Material*)mapped;
			for (size_t i = 0; i < material_count; ++i)
			{
				writeaddr[i] = material_manager->resources[i].resource;
				//assert(material_manager->resources[i].resource.base_color != -1);
			}
			vmaUnmapMemory(context->allocator, scene.material_buffer.allocation);

			VkDescriptorBufferInfo buf_info{};
			buf_info.buffer = scene.material_buffer.buffer;
			buf_info.offset = 0;
			buf_info.range = VK_WHOLE_SIZE;
			buf_infos.push_back(buf_info);

			VkWriteDescriptorSet buf_write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			buf_write.descriptorCount = 1;
			buf_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			buf_write.dstArrayElement = 0;
			buf_write.dstBinding = BINDLESS_UNIFORM_BINDING;
			buf_write.dstSet = bindless_descriptor_set;
			buf_write.pBufferInfo = &buf_infos.back();
			writes.push_back(buf_write);
		}
		vkUpdateDescriptorSets(context->device, (u32)writes.size(), writes.data(), 0, nullptr);
	}

	// Find first active camera
	for (auto [cam] : ecs->filter<Camera_Component>())
	{
		scene.active_camera = cam;
		break;
	}

	u32 aligned_size = vkinit::aligned_size(
		sizeof(Camera_Data), 
		(u32)context->physical_device_properties.properties.limits.minUniformBufferOffsetAlignment
	);
	/*gpu_camera_data = context->allocate_buffer(aligned_size * FRAMES_IN_FLIGHT,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);*/
	gpu_camera_data = context->create_gpu_buffer(
		aligned_size * 2 * FRAMES_IN_FLIGHT,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
	VkCommandBuffer cmd = get_current_frame_command_buffer();
	vk_begin_command_buffer(cmd);

	// Prefilter envmap
	prefiltered_envmap = prefilter_envmap(cmd, environment_map);

	glm::vec3 scene_bbmin = glm::vec3(INFINITY);
	glm::vec3 scene_bbmax = glm::vec3(-INFINITY);
	// Create vertex / index buffers
	for (auto [mesh] : ecs->filter<Static_Mesh_Component>())
	{
		Mesh* m = mesh->manager->get_resource_with_id(mesh->mesh_id);
		if (m->vertex_buffer.buffer != VK_NULL_HANDLE && m->index_buffer.buffer != VK_NULL_HANDLE)
			continue;

		scene_bbmin = glm::min(m->bbmin, scene_bbmin);
		scene_bbmax = glm::max(m->bbmax, scene_bbmax);
		create_vertex_buffer(m, cmd, context);
		create_index_buffer(m, cmd, context);
		
		u32 prim_count = (u32)m->primitives.size();
		m->instance_data_buffer = context->create_gpu_buffer((u32)(m->primitives.size() * sizeof(Primitive_Info)), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		std::vector<Primitive_Info> primitive_infos(prim_count);
		std::vector<VkDrawIndexedIndirectCommand> indirect_draw_commands(prim_count);
		for (u32 i = 0; i < prim_count; ++i)
		{
			u32 mesh_id = mesh->mesh_id;
			u32 index = ((m->primitives[i].material_id & 0x3FF) << 14) | (mesh_id & 0x3FFF);

			indirect_draw_commands[i].indexCount = m->primitives[i].vertex_count;
			indirect_draw_commands[i].instanceCount = 1;
			indirect_draw_commands[i].firstIndex = m->primitives[i].vertex_offset;
			indirect_draw_commands[i].vertexOffset = 0;
			indirect_draw_commands[i].firstInstance = index;

			primitive_infos[i].material_index = m->primitives[i].material_id;
			primitive_infos[i].vertex_count = m->primitives[i].vertex_count;
			primitive_infos[i].vertex_offset = m->primitives[i].vertex_offset;
		}
		indirect_draw_buffer.update_staging_buffer(context->allocator, current_frame_index, indirect_draw_commands.data(), indirect_draw_commands.size() * sizeof(indirect_draw_commands[0]));
		indirect_draw_buffer.upload(cmd, current_frame_index);

		m->instance_data_buffer.update_staging_buffer(context->allocator, current_frame_index, primitive_infos.data(), primitive_infos.size() * sizeof(primitive_infos[0]));
		m->instance_data_buffer.upload(cmd, current_frame_index);
	}

	memory_barrier(cmd,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

	for (auto [mesh] : ecs->filter<Static_Mesh_Component>())
	{
		Mesh* m = mesh->manager->get_resource_with_id(mesh->mesh_id);
		create_bottom_level_acceleration_structure(m);
		build_bottom_level_acceleration_structure(m, cmd);
	}
	memory_barrier(cmd,
		VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
	create_top_level_acceleration_structure(ecs, cmd);

	vkEndCommandBuffer(cmd);
	vk_command_buffer_single_submit(cmd);

	{
		// Update vertex / index buffers
		size_t mesh_count = mesh_manager->resources.size();
		constexpr u32 buffer_count_per_mesh = 3;
		std::vector<VkWriteDescriptorSet> writes(mesh_count * buffer_count_per_mesh);
		std::vector<VkDescriptorBufferInfo> buf_infos(mesh_count * buffer_count_per_mesh);
		for (size_t i = 0; i < mesh_count; ++i)
		{
			Mesh* m = &mesh_manager->resources[i].resource;

			VkDescriptorBufferInfo buffer_info[buffer_count_per_mesh];
			buffer_info[0].buffer = m->vertex_buffer.buffer;
			buffer_info[0].offset = 0;
			buffer_info[0].range = VK_WHOLE_SIZE;
			buffer_info[1].buffer = m->index_buffer.buffer;
			buffer_info[1].offset = 0;
			buffer_info[1].range = VK_WHOLE_SIZE;
			buffer_info[2].buffer = m->instance_data_buffer.gpu_buffer.buffer;
			buffer_info[2].offset = 0;
			buffer_info[2].range = VK_WHOLE_SIZE;

			VkWriteDescriptorSet vwrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			VkWriteDescriptorSet iwrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			VkWriteDescriptorSet instance_write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			vwrite.descriptorCount = 1;
			vwrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			vwrite.dstArrayElement = (u32)i;
			vwrite.dstBinding = BINDLESS_VERTEX_BINDING;
			vwrite.dstSet = bindless_descriptor_set;
			buf_infos[i * buffer_count_per_mesh] = buffer_info[0];
			vwrite.pBufferInfo = &buf_infos[i * buffer_count_per_mesh];

			iwrite.descriptorCount = 1;
			iwrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			iwrite.dstArrayElement = (u32)i;
			iwrite.dstBinding = BINDLESS_INDEX_BINDING;
			iwrite.dstSet = bindless_descriptor_set;
			buf_infos[i * buffer_count_per_mesh + 1] = buffer_info[1];
			iwrite.pBufferInfo = &buf_infos[i * buffer_count_per_mesh + 1];

			instance_write.descriptorCount = 1;
			instance_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			instance_write.dstArrayElement = (u32)i;
			instance_write.dstBinding = BINDLESS_INSTANCE_DATA_BINDING;
			instance_write.dstSet = bindless_descriptor_set;
			buf_infos[i * buffer_count_per_mesh + 2] = buffer_info[2];
			instance_write.pBufferInfo = &buf_infos[i * buffer_count_per_mesh + 2];

			writes[i * buffer_count_per_mesh + 0] = vwrite;
			writes[i * buffer_count_per_mesh + 1] = iwrite;
			writes[i * buffer_count_per_mesh + 2] = instance_write;
		}
		vkUpdateDescriptorSets(context->device, (u32)writes.size(), writes.data(), 0, nullptr);
	}

	{
		// Update ray tracing pipeline instance data
	}

	vkQueueWaitIdle(context->graphics_queue);

	{
		// Bake light probes
		VkCommandBuffer cmd = get_current_frame_command_buffer();
		vk_begin_command_buffer(get_current_frame_command_buffer());
		probe_system.init(context, bindless_descriptor_set, bindless_set_layout, &gpu_camera_data, &scene, cmd, &global_constants_buffer);
		probe_system.init_probe_grid(scene_bbmin, scene_bbmax);
		//probe_system.bake(cmd, &cubemap, bilinear_sampler_clamp);
		vkEndCommandBuffer(cmd);
		vk_command_buffer_single_submit(cmd);
		vkQueueWaitIdle(context->graphics_queue);
	}
}

void full_barrier(VkCommandBuffer cmd)
{
	VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;

	VkDependencyInfo dep_info{};
	dep_info.memoryBarrierCount = 1;
	dep_info.pMemoryBarriers = &barrier;
	dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

	vkCmdPipelineBarrier2(cmd, &dep_info);
}

void Renderer::pre_frame()
{
	g_garbage_collector->collect();


	scene.previous_frame_camera = scene.current_frame_camera;
	// Update cameras
	if (scene.active_camera->dirty)
	{
		frames_accumulated = 0;
		scene.active_camera->dirty = false;
	}

	scene.current_frame_camera.view = scene.active_camera->get_view_matrix();
	scene.current_frame_camera.proj = scene.active_camera->get_projection_matrix(aspect_ratio, 0.1f, 1000.f);
	scene.current_frame_camera.viewproj = scene.current_frame_camera.proj * scene.current_frame_camera.view;
	scene.current_frame_camera.inverse_proj = glm::inverse(scene.current_frame_camera.proj);
	scene.current_frame_camera.inverse_view = glm::inverse(scene.current_frame_camera.view);
	scene.current_frame_camera.frame_index = glm::uvec4((u32)frame_counter);

	gpu_camera_data.update_staging_buffer(context->allocator, current_frame_index, &scene.current_frame_camera, sizeof(Camera_Data));
	gpu_camera_data.update_staging_buffer(context->allocator, current_frame_index, &scene.previous_frame_camera, sizeof(Camera_Data), sizeof(Camera_Data));

	float unproject_y = 1.0f / tan(glm::radians(scene.active_camera->fov * 0.5f));
	float unproject = 1.0f / (0.5f * (float)window_height * unproject_y);
	glm::vec3 sun_direction = math::polar_to_unit_vec(glm::radians(g_settings.sun_azimuth), glm::radians(g_settings.sun_zenith));
	global_constants_data->sun_direction = sun_direction;
	global_constants_data->exposure = g_settings.exposure;
	global_constants_data->sun_intensity = g_settings.sun_intensity;
	global_constants_data->sun_color = glm::vec3(1.0f);
	global_constants_data->unproject = unproject;
	global_constants_data->min_rect_dim_mul_unproject = min((float)window_width, (float)window_height) * unproject;
	global_constants_data->prepass_blur_radius = g_settings.prepass_blur_radius;
	global_constants_data->blur_radius = g_settings.blur_radius;
	global_constants_data->post_blur_radius_scale = g_settings.post_blur_radius_scale;
	global_constants_data->temporal_accumulation = (int)g_settings.temporal_accumulation;
	global_constants_data->history_fix = (int)g_settings.history_fix;
	global_constants_data->hit_dist_params = g_settings.hit_distance_params;
	global_constants_data->temporal_filtering_mode = (int)g_settings.temporal_filter;
	global_constants_data->bicubic_sharpness = g_settings.bicubic_sharpness;
	global_constants_data->screen_output = g_settings.screen_output;
	global_constants_data->plane_distance_sensitivity = g_settings.plane_dist_sensitivity / 100.0f;
	global_constants_data->camera_origin = scene.active_camera->origin;
	global_constants_data->frame_number = (u32)frame_counter;
	global_constants_data->blur_kernel_rotation_mode = (u32)g_settings.blur_kernel_rotation_mode;
	global_constants_data->blur_radius = g_settings.blur_radius;
	global_constants_data->frame_num_scaling = (u32)g_settings.frame_num_scaling;
	global_constants_data->hit_distance_scaling = (u32)g_settings.hit_dist_scaling;
	global_constants_data->use_gaussian_weight = (u32)g_settings.use_gaussian_weight;
	global_constants_data->screen_space_sampling = (u32)g_settings.screen_space_sampling;
	global_constants_data->use_quadratic_distribution = (u32)g_settings.use_quadratic_distribution;
	global_constants_data->use_geometry_weight = (u32)g_settings.use_geometry_weight;
	global_constants_data->use_normal_weight = (u32)g_settings.use_normal_weight;
	global_constants_data->use_hit_distance_weight = (u32)g_settings.use_hit_distance_weight;
	global_constants_data->plane_dist_norm_scale = g_settings.plane_dist_norm_scale;
	global_constants_data->lobe_percentage = g_settings.lobe_percentage;
	global_constants_data->hit_distance_scale = g_settings.hit_distance_scale;
	global_constants_data->stabilization_strength = g_settings.stabilization_strength;
	global_constants_data->use_ycocg_color_space = (u32)g_settings.use_ycocg_color_space;
	global_constants_data->taa = (u32)g_settings.taa;
	global_constants_data->use_probe_normal_weight = (u32)g_settings.use_probe_normal_weight;
	global_constants_data->roughness_override = g_settings.roughness_override;
	global_constants_data->lobe_trim_factor = g_settings.lobe_trim_factor;
	global_constants_data->demodulate_specular = (u32)g_settings.demodulate_specular;
	global_constants_data->spec_accum_base_power = g_settings.spec_accum_base_power;
	global_constants_data->spec_accum_curve = g_settings.spec_accum_curve;
	global_constants_data->use_roughness_override = (u32)g_settings.use_roughness_override;
	global_constants_data->indirect_diffuse = (u32)g_settings.indirect_diffuse;
	global_constants_data->indirect_specular = (u32)g_settings.indirect_specular;
}

void Renderer::begin_frame()
{
	cpu_frame_begin = timer->get_current_time();

	vkWaitForFences(context->device, 1, &context->frame_objects[current_frame_index].fence, VK_TRUE, UINT64_MAX);
	double after_fence = timer->get_current_time();
	//printf("fence wait time: %f\n", after_fence - cpu_frame_begin);
	vkResetFences(context->device, 1, &context->frame_objects[current_frame_index].fence);
	vkAcquireNextImageKHR(context->device, context->swapchain,
		UINT64_MAX, context->frame_objects[current_frame_index].image_available_sem,
		VK_NULL_HANDLE, &swapchain_image_index);

	VkCommandBuffer cmd = get_current_frame_command_buffer();
	VkImage next_image = context->swapchain_images[swapchain_image_index];


	// Time from two frames ago
	u64 query_results[2];
	vkGetQueryPoolResults(context->device, query_pools[current_frame_index], 0, (u32)std::size(query_results), sizeof(query_results), query_results, sizeof(query_results[0]), VK_QUERY_RESULT_64_BIT);
	double frame_gpu_begin = double(query_results[0]) * context->physical_device_properties.properties.limits.timestampPeriod;
	double frame_gpu_end = double(query_results[1]) * context->physical_device_properties.properties.limits.timestampPeriod;

	current_frame_gpu_time = frame_gpu_end - frame_gpu_begin;
	vkResetCommandBuffer(cmd, 0);
	vk_begin_command_buffer(cmd);

	vkCmdResetQueryPool(cmd, query_pools[current_frame_index], 0, 128);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pools[current_frame_index], 0);

	// Update camera data
	gpu_camera_data.upload(cmd, current_frame_index);
	
	vkinit::memory_barrier2(
		cmd,
		VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR
	);

	vk_transition_layout(cmd, next_image,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_ACCESS_HOST_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT
	);
	
	vk_transition_layout(cmd, final_output.image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
	);
}

Vk_Pipeline Renderer::create_raster_graphics_pipeline(const char* vertex_shader_path, const char* fragment_shader_path, bool use_bindless_layout, Raster_Options opt)
{
	Shader vertex_shader, fragment_shader;
	bool success = load_shader_from_file(&vertex_shader, context->device, vertex_shader_path);
	assert(success);
	success = load_shader_from_file(&fragment_shader, context->device, fragment_shader_path);
	assert(success);

	Shader shaders[] = { vertex_shader, fragment_shader };

	VkDescriptorSetLayout desc_0 = context->create_descriptor_set_layout((u32)std::size(shaders), shaders);

	VkDescriptorSetLayout descriptor_set_layouts[4] = {  };
	descriptor_set_layouts[0] = desc_0;
	u32 layout_count = use_bindless_layout ? 2 : 1;

	if (use_bindless_layout)
		descriptor_set_layouts[1] = bindless_set_layout;

	Vk_Pipeline pp = context->create_raster_pipeline(vertex_shader.shader, fragment_shader.shader, layout_count, descriptor_set_layouts, opt);

	VkDescriptorUpdateTemplate update_template = context->create_descriptor_update_template((u32)std::size(shaders), shaders, VK_PIPELINE_BIND_POINT_GRAPHICS, pp.layout);
	pp.update_template = update_template;

	vkDestroyShaderModule(context->device, vertex_shader.shader, nullptr);
	vkDestroyShaderModule(context->device, fragment_shader.shader, nullptr);

	g_garbage_collector->push([=]()
		{
			vkDestroyPipeline(context->device, pp.pipeline, nullptr);
			vkDestroyPipelineLayout(context->device, pp.layout, nullptr);
			vkDestroyDescriptorSetLayout(context->device, desc_0, nullptr);
			vkDestroyDescriptorUpdateTemplate(context->device, update_template, nullptr);
		}, Garbage_Collector::SHUTDOWN);

	return pp;
}

void Renderer::render_gbuffer()
{
	VkCommandBuffer cmd = get_current_frame_command_buffer();

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, primary_ray_pipeline.pipeline.pipeline);


}

void Renderer::trace_primary_rays()
{
	VkCommandBuffer cmd = get_current_frame_command_buffer();
	// Bind pipeline
	
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, primary_ray_pipeline.pipeline.pipeline);
	// Update descriptor sets (use same set as path tracer?)
	// Trace rays
}

void Renderer::end_frame()
{
	VkCommandBuffer cmd = get_current_frame_command_buffer();

	VkImageBlit region{};
	VkImageSubresourceLayers src_layer{};
	src_layer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	src_layer.baseArrayLayer = 0;
	src_layer.mipLevel = 0;
	src_layer.layerCount = 1;
	region.srcSubresource = src_layer;
	region.srcOffsets[0] = { 0, 0, 0 };
	region.dstOffsets[0] = { 0, 0, 0 };
	region.srcOffsets[1] = { window_width, window_height, 1 };
	region.dstOffsets[1] = { window_width, window_height, 1 };
	VkImageSubresourceLayers dst_layer{};
	dst_layer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	dst_layer.baseArrayLayer = 0;
	dst_layer.mipLevel = 0;
	dst_layer.layerCount = 1;
	region.dstSubresource = dst_layer;
	vk_transition_layout(cmd, final_output.image,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT
	);
	VkImage next_image = context->swapchain_images[swapchain_image_index];
	vkCmdBlitImage(cmd, 
		final_output.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, next_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);
	vk_transition_layout(
		cmd, next_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT
	);
	
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pools[current_frame_index], 1);

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;
	submit.signalSemaphoreCount = 1;
	submit.waitSemaphoreCount = 1;
	submit.pWaitDstStageMask = &wait_stage;
	submit.pWaitSemaphores = &context->frame_objects[current_frame_index].image_available_sem;
	submit.pSignalSemaphores = &context->frame_objects[current_frame_index].render_finished_sem;

	vkEndCommandBuffer(cmd);

	vkQueueSubmit(context->graphics_queue, 1, &submit, context->frame_objects[current_frame_index].fence);

	VkPresentInfoKHR present_info{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &context->swapchain;
	present_info.pImageIndices = &swapchain_image_index;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &context->frame_objects[current_frame_index].render_finished_sem;
	vkQueuePresentKHR(context->graphics_queue, &present_info);

	cpu_frame_end = timer->get_current_time();

	vkDeviceWaitIdle(context->device); // Synchronization debugging

	char title[256];
	sprintf(title, "cpu time: %.2f ms, gpu time: %.2f ms, mode: %s", (cpu_frame_end - cpu_frame_begin) * 1000.0, current_frame_gpu_time * 1e-6, g_settings.rendering_mode == Rendering_Mode::REFERENCE_PATH_TRACER ? "path tracer" : "hybrid");
	SDL_SetWindowTitle(platform->window.window, title);

	frame_counter++;
	current_frame_index = (current_frame_index + 1) % FRAMES_IN_FLIGHT;
	previous_frame_gbuffer_index = current_frame_gbuffer_index;
	current_frame_gbuffer_index = (current_frame_gbuffer_index + 1) % GBUFFER_LAYERS;
}

void Renderer::draw(ECS* ecs, float dt)
{
	VkCommandBuffer cmd = get_current_frame_command_buffer();

	probe_system.bake(cmd, &cubemap, samplers[BILINEAR_SAMPLER_CLAMP]);

	if(g_settings.rendering_mode == Rendering_Mode::REFERENCE_PATH_TRACER)
		trace_rays(cmd);
	else if (g_settings.rendering_mode == Rendering_Mode::HYBRID_RENDERER)
		rasterize(cmd, ecs);
	/*else if (render_mode == SIDE_BY_SIDE)
	{
		trace_rays(cmd);
		full_barrier(cmd);
		rasterize(cmd, ecs);
	}*/

	composite_and_tonemap(cmd);

	if (g_settings.visualize_probes)
	{
		VkRect2D render_area = vkinit::rect_2d(0, 0, window_width, window_height);
		VkRenderingAttachmentInfo color = vkinit::rendering_attachment_info(
			final_output.image_view, 
			VK_IMAGE_LAYOUT_GENERAL, 
			VK_ATTACHMENT_LOAD_OP_LOAD, 
			VK_ATTACHMENT_STORE_OP_STORE);
		VkRenderingAttachmentInfo depth = vkinit::rendering_attachment_info(
			framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE);
		VkRenderingInfo rendering_info = vkinit::rendering_info(render_area, 1, &color, &depth);

		vkCmdBeginRendering(cmd, &rendering_info);

		VkViewport viewport = vkinit::viewport(0.f, 0.f, (float)window_width, (float)window_height, 0.f, 1.f);
		vkCmdSetScissor(cmd, 0, 1, &render_area);
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		probe_system.debug_render(cmd);
		vkCmdEndRendering(cmd);

		VkImageSubresourceRange range = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
		VkImageMemoryBarrier2 img_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
		img_barrier.image = final_output.image;
		img_barrier.subresourceRange = range;
		img_barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		img_barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		img_barrier.oldLayout = img_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		img_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		img_barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		VkDependencyInfo deps = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		deps.imageMemoryBarrierCount = 1;
		deps.pImageMemoryBarriers = &img_barrier;

		vkCmdPipelineBarrier2(cmd, &deps);
	}

	ui_overlay.update_and_render(cmd, dt);
	
	frames_accumulated++;
}

void Renderer::trace_rays(VkCommandBuffer cmd)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelines[PATH_TRACER_PIPELINE].pipeline);

	// Push descriptors
	u32 frame_index = frame_counter % FRAMES_IN_FLIGHT;
	{
		Descriptor_Info descriptor_info[] =
		{
			Descriptor_Info(0, framebuffer.render_targets[PATH_TRACER_COLOR].images[0].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(scene.tlas.value().acceleration_structure),
			Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_WRAP], environment_map.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], cubemap.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE)
		};
		vkCmdPushDescriptorSetWithTemplateKHR(cmd, descriptor_update_template, pipelines[PATH_TRACER_PIPELINE].layout, 0, descriptor_info);
	}
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelines[PATH_TRACER_PIPELINE].layout, 1, 1, &bindless_descriptor_set, 0, nullptr);
	vkCmdPushConstants(cmd, pipelines[PATH_TRACER_PIPELINE].layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(frames_accumulated), &frames_accumulated);

	// Trace rays

	VkDeviceAddress base = context->get_buffer_device_address(shader_binding_table);
	VkStridedDeviceAddressRegionKHR raygen_region{};
	raygen_region.deviceAddress = base + 0 * context->device_sbt_alignment;
	raygen_region.stride = 32;
	raygen_region.size = 32;

	VkStridedDeviceAddressRegionKHR miss_region{};
	miss_region.deviceAddress = base + 1 * context->device_sbt_alignment;
	miss_region.stride = 32;
	miss_region.size = 32;

	VkStridedDeviceAddressRegionKHR hit_region{};
	hit_region.deviceAddress = base + 2 * context->device_sbt_alignment;
	hit_region.stride = 32;
	hit_region.size = 32;

	VkStridedDeviceAddressRegionKHR callable_region{};

	vkCmdTraceRaysKHR(
		cmd,
		&raygen_region,
		&miss_region,
		&hit_region,
		&callable_region,
		window_width, window_height, 1
	);

	VkMemoryBarrier memory_barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		1, &memory_barrier,
		0, nullptr,
		0, nullptr
	);
}

void Renderer::composite_and_tonemap(VkCommandBuffer cmd)
{	
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[COMPOSITION_PIPELINE].pipeline);
		// Go for groupsize 8x8?
		constexpr u32 group_size = 8;
		glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
		glm::uvec3 group_count = (size + (group_size - 1)) & ~(group_size - 1);
		group_count /= group_size;

		Descriptor_Info descriptor_info[] = {
			Descriptor_Info(0, framebuffer.render_targets[PATH_TRACER_COLOR].images[0].image_view,  VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[RASTER_COLOR].images[0].image_view,  VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view,  VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[BASECOLOR_METALNESS].images[current_frame_gbuffer_index].image_view,  VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view,  VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[INDIRECT_DIFFUSE].images[0].image_view ,VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[TEMPORAL_STABILIZATION_HISTORY].images[current_frame_gbuffer_index].image_view ,VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[COMPOSITION_OUTPUT].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], history_fix.radiance_image.image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], history_fix.view_z_image.image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
			Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[DEBUG].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(probe_system.probe_samples.buffer, 0, VK_WHOLE_SIZE),
			Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view ,VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[INDIRECT_SPECULAR].images[0].image_view ,VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[TEMPORAL_STABILIZATION_HISTORY_SPEC].images[current_frame_gbuffer_index].image_view ,VK_IMAGE_LAYOUT_GENERAL),
			//Descriptor_Info(0, brdf_lut.image_view, VK_IMAGE_LAYOUT_GENERAL)
			//Descriptor_Info(0, prefiltered_envmap.image_view, VK_IMAGE_LAYOUT_GENERAL)
		};
		vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[COMPOSITION_PIPELINE].update_template, pipelines[COMPOSITION_PIPELINE].layout, 0, descriptor_info);

		struct
		{
			glm::ivec2 size;
			float split_pos;
			u32 history_lod;
			vec3 probe_min;
			float probe_spacing;
			ivec3 probe_counts;
		} pc;
		pc.size = glm::ivec2(size.x, size.y);
		pc.split_pos = g_settings.rendering_mode == Rendering_Mode::REFERENCE_PATH_TRACER ? 1.0f : 0.0f;
		pc.history_lod = magic_uint % 5;
		pc.probe_min = probe_system.bbmin;
		pc.probe_counts = probe_system.probe_counts;
		pc.probe_spacing = probe_system.probe_spacing;
		vkCmdPushConstants(cmd, pipelines[COMPOSITION_PIPELINE].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
		vkCmdDispatch(cmd, group_count.x, group_count.y, group_count.z);
	}


	VkMemoryBarrier memory_barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		1, &memory_barrier,
		0, nullptr,
		0, nullptr
	);

	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[TONEMAP_AND_TAA].pipeline);
		// Go for groupsize 8x8?
		constexpr u32 group_size = 8;
		glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
		glm::uvec3 group_count = (size + (group_size - 1)) & ~(group_size - 1);
		group_count /= group_size;

		Descriptor_Info descriptor_info[] = {
			Descriptor_Info(0, framebuffer.render_targets[COMPOSITION_OUTPUT].images[current_frame_gbuffer_index].image_view,  VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, final_output.image_view,  VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[TAA_OUTPUT].images[previous_frame_gbuffer_index].image_view,  VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[TAA_OUTPUT].images[current_frame_gbuffer_index].image_view,  VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
			Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
			Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
		};

		vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[TONEMAP_AND_TAA].update_template, pipelines[TONEMAP_AND_TAA].layout, 0, descriptor_info);

		struct
		{
			glm::ivec2 size;
		} pc;
		pc.size = glm::ivec2(size.x, size.y);

		vkCmdPushConstants(cmd, pipelines[TONEMAP_AND_TAA].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
		vkCmdDispatch(cmd, group_count.x, group_count.y, group_count.z);
	}

	memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		1, &memory_barrier,
		0, nullptr,
		0, nullptr
	);
}

void Renderer::rasterize(VkCommandBuffer cmd, ECS* ecs)
{
	{
		VkClearColorValue clr = {};
		VkImageSubresourceRange range = {};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseArrayLayer = 0;
		range.baseMipLevel = 0;
		range.layerCount = 1;
		range.levelCount = 1;
		vkCmdClearColorImage(cmd, framebuffer.render_targets[DEBUG].images[current_frame_gbuffer_index].image, VK_IMAGE_LAYOUT_GENERAL, &clr, 1, &range);
	}

	if (needs_history_clear)
	{
		VkClearColorValue clr = {};
		VkImageSubresourceRange range = {};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseArrayLayer = 0;
		range.baseMipLevel = 0;
		range.layerCount = 1;
		range.levelCount = 1;

		vkCmdClearColorImage(cmd, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[previous_frame_gbuffer_index].image, VK_IMAGE_LAYOUT_GENERAL, &clr, 1, &range);

		needs_history_clear = false;
	}

	VkClearValue clear_value{};
	clear_value.color = { 0.2f, 0.5f, 0.1f };

	VkClearValue depth_clear{};
	depth_clear.depthStencil.depth = 0.f;


	VkRenderingAttachmentInfo attachment_infos[4] = {};
	attachment_infos[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	attachment_infos[0].imageView = framebuffer.render_targets[RASTER_COLOR].images[0].image_view;
	attachment_infos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	attachment_infos[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment_infos[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment_infos[0].clearValue = clear_value;

	attachment_infos[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	attachment_infos[1].imageView = framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view;
	attachment_infos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	attachment_infos[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment_infos[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment_infos[1].clearValue = { 0.0f, 0.0f, 0.0f, 0.0f };

	attachment_infos[2].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	attachment_infos[2].imageView = framebuffer.render_targets[BASECOLOR_METALNESS].images[current_frame_gbuffer_index].image_view;
	attachment_infos[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	attachment_infos[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment_infos[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment_infos[2].clearValue = { 0.0f, 0.0f, 0.0f, 0.0f };


	attachment_infos[3].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	attachment_infos[3].imageView = framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view;
	attachment_infos[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	attachment_infos[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment_infos[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment_infos[3].clearValue = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingAttachmentInfo depth_attachment_info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	depth_attachment_info.imageView = framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view;
	depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment_info.clearValue = depth_clear;

	VkRect2D render_area = { {0, 0}, {(u32)window_width, (u32)window_height} };
	VkRenderingInfo rendering_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
	rendering_info.renderArea = render_area;
	rendering_info.layerCount = 1;
	rendering_info.colorAttachmentCount = (u32)std::size(attachment_infos);
	rendering_info.pColorAttachments = attachment_infos;
	rendering_info.pDepthAttachment = &depth_attachment_info;
	vkCmdBeginRendering(cmd, &rendering_info);

#if 1
	VkViewport viewport{};
	viewport.height = -(float)window_height;
	viewport.width = (float)window_width;
	viewport.minDepth = 0.0f; 
	viewport.maxDepth = 1.0f;
	viewport.x = 0.f;
	viewport.y = (float)window_height;
#else
	VkViewport viewport{};
	viewport.height = (float)window_height;
	viewport.width = (float)window_width;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.x = 0.f;
	viewport.y = 0.f;
#endif
	//goto end_rendering;
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &render_area);

	{
		// Draw skybox
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[SKYBOX_PIPELINE].pipeline);
		{
			Descriptor_Info descriptor_info[] =
			{
				Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_WRAP], cubemap.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			};
			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[SKYBOX_PIPELINE].update_template, pipelines[SKYBOX_PIPELINE].layout, 0, descriptor_info);
		}

		vkCmdDraw(cmd, 36, 1, 0, 0);
	}

	{
		// Draw gbuffer + direct lighting

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[RASTER_PIPELINE].pipeline);

		struct
		{
			glm::uvec3 probe_counts;
			float probe_spacing;
			glm::vec3 probe_min;
			glm::vec2 jitter;
			glm::vec2 inverse_screen_size;
		} pc;

		pc.probe_counts = probe_system.probe_counts;
		pc.probe_spacing = probe_system.probe_spacing;
		pc.probe_min = probe_system.bbmin;
		pc.jitter = g_settings.jitter ? halton_sequence[frame_counter % TAA_SAMPLE_COUNT] : glm::vec2(0.5f);
		pc.inverse_screen_size = glm::vec2(1.0f / (float)window_width, 1.0f / (float)window_height);
		vkCmdPushConstants(cmd, pipelines[RASTER_PIPELINE].layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
		{
			Descriptor_Info descriptor_info[] =
			{
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], brdf_lut.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_WRAP], prefiltered_envmap.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
				Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_WRAP], environment_map.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], cubemap.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
				Descriptor_Info(probe_system.probe_samples.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(scene.tlas.value().acceleration_structure),
				Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE)
			};
			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[RASTER_PIPELINE].update_template, pipelines[RASTER_PIPELINE].layout, 0, descriptor_info);
		}
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[RASTER_PIPELINE].layout, 1, 1, &bindless_descriptor_set, 0, nullptr);


		for (auto [a] : ecs->filter<Static_Mesh_Component>())
		{
			Mesh* mesh = a->manager->get_resource_with_id(a->mesh_id);
			i32 mat_id = mesh->primitives[0].material_id;
			i32 mesh_id = a->mesh_id;

			u32 index = ((mat_id & 0x3FF) << 14) | (mesh_id & 0x3FFF);

			vkCmdBindIndexBuffer(cmd, mesh->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			//vkCmdDrawIndexed(cmd, (u32)mesh->indices.size(), 1, 0, 0, index);
			vkCmdDrawIndexedIndirect(cmd, indirect_draw_buffer.gpu_buffer.buffer, 0, (u32)mesh->primitives.size(), sizeof(VkDrawIndexedIndirectCommand));
		}
	}
	//probe_system.debug_render(cmd);

	//ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);


	VkMemoryBarrier memory_barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		1, &memory_barrier,
		0, nullptr,
		0, nullptr
	);

	{
		// Trace indirect diffuse rays

		constexpr glm::uvec3 group_size = glm::uvec3(4, 8, 1);
		glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
		glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
		group_count /= group_size;

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[INDIRECT_DIFFUSE_PIPELINE].pipeline);

		Descriptor_Info descriptor_info[] =
		{
			Descriptor_Info(0, framebuffer.render_targets[INDIRECT_DIFFUSE].images[0].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_WRAP], framebuffer.render_targets[BASECOLOR_METALNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(scene.tlas.value().acceleration_structure),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], cubemap.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], blue_noise[g_settings.animate_noise ? frame_counter % BLUE_NOISE_TEXTURE_COUNT : 0].image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
			//Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], blue_noise[0].image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),

		};

		struct
		{
			glm::ivec2 size;
			u32 frame_number;
			u32 frames_accumulated;
		} pc;

		pc.size = glm::ivec2(window_width, window_height);
		pc.frame_number = (u32)frame_counter;
		pc.frames_accumulated = frames_accumulated;
		vkCmdPushConstants(cmd, pipelines[INDIRECT_DIFFUSE_PIPELINE].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[INDIRECT_DIFFUSE_PIPELINE].layout, 1, 1, &bindless_descriptor_set, 0, nullptr);
		vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[INDIRECT_DIFFUSE_PIPELINE].update_template, pipelines[INDIRECT_DIFFUSE_PIPELINE].layout, 0, descriptor_info);

		vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
	}

	{
		// Trace indirect specular rays
		constexpr glm::uvec3 group_size = glm::uvec3(4, 8, 1);
		glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
		glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
		group_count /= group_size;

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[INDIRECT_SPECULAR_PIPELINE].pipeline);

		Descriptor_Info descriptor_info[] =
		{
			Descriptor_Info(0, framebuffer.render_targets[INDIRECT_SPECULAR].images[0].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_WRAP], framebuffer.render_targets[BASECOLOR_METALNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			Descriptor_Info(scene.tlas.value().acceleration_structure),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], cubemap.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], blue_noise_vec2[g_settings.animate_noise ? frame_counter % BLUE_NOISE_TEXTURE_COUNT : 0].image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
			Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
		};

		struct
		{
			glm::ivec2 size;
			u32 frame_number;
			u32 frames_accumulated;
		} pc;

		pc.size = glm::ivec2(window_width, window_height);
		pc.frame_number = (u32)frame_counter;
		pc.frames_accumulated = frames_accumulated;
		vkCmdPushConstants(cmd, pipelines[INDIRECT_SPECULAR_PIPELINE].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[INDIRECT_SPECULAR_PIPELINE].layout, 1, 1, &bindless_descriptor_set, 0, nullptr);
		vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[INDIRECT_SPECULAR_PIPELINE].update_template, pipelines[INDIRECT_SPECULAR_PIPELINE].layout, 0, descriptor_info);

		vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
	}

	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		0,
		1, &memory_barrier,
		0, nullptr,
		0, nullptr
	);

	{
		// Denoise indirect diffuse and specular
		{
			// Pre-blur pass diffuse

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[PRE_BLUR].pipeline);

			Descriptor_Info descriptor_info[] =
			{
				//Descriptor_Info(0, framebuffer.render_targets[INDIRECT_DIFFUSE].images[0].image_view, VK_IMAGE_LAYOUT_GENERAL),
				//Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				//Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				//Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				//Descriptor_Info(0, framebuffer.render_targets[DENOISER_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				//Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),

				Descriptor_Info(0, framebuffer.render_targets[INDIRECT_DIFFUSE].images[0].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DEBUG].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			};

			struct
			{
				glm::mat2 rot;
				glm::ivec2 size;
				float depth_scale;
				float blur_radius;
				float blur_radius_scale;
			} pc;

			float angle = 0.5f * (float)M_PI * van_der_corput_base_2[(frame_counter + 17) % 64];

			//printf("angle: %f\n", angle);
			//angle = 0.0f;

			pc.size = glm::ivec2(window_width, window_height);
			pc.rot = glm::mat2(cosf(angle), sinf(angle), -sinf(angle), cosf(angle));
			pc.depth_scale = scene.current_frame_camera.proj[3][2];
			pc.blur_radius = g_settings.prepass_blur_radius;
			pc.blur_radius_scale = 1.0;
			vkCmdPushConstants(cmd, pipelines[PRE_BLUR].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[PRE_BLUR].update_template, pipelines[PRE_BLUR].layout, 0, descriptor_info);
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);

		}

		{
			// Pre-blur pass specular

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[PRE_BLUR].pipeline);

			Descriptor_Info descriptor_info[] =
			{
				Descriptor_Info(0, framebuffer.render_targets[INDIRECT_SPECULAR].images[0].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_SPECULAR_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DEBUG].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			};

			struct
			{
				glm::mat2 rot;
				glm::ivec2 size;
				float depth_scale;
				float blur_radius;
				float blur_radius_scale;
			} pc;

			float angle = 0.5f * (float)M_PI * van_der_corput_base_2[(frame_counter + 17) % 64];

			//printf("angle: %f\n", angle);
			//angle = 0.0f;

			pc.size = glm::ivec2(window_width, window_height);
			pc.rot = glm::mat2(cosf(angle), sinf(angle), -sinf(angle), cosf(angle));
			pc.depth_scale = scene.current_frame_camera.proj[3][2];
			pc.blur_radius = g_settings.prepass_blur_radius;
			pc.blur_radius_scale = 1.0;
			vkCmdPushConstants(cmd, pipelines[PRE_BLUR].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[PRE_BLUR].update_template, pipelines[PRE_BLUR].layout, 0, descriptor_info);
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);

		}

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			0,
			1, &memory_barrier,
			0, nullptr,
			0, nullptr
		);

		{
			// Temporal accumulation pass

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[TEMPORAL_ACCUMULATION].pipeline);

			Descriptor_Info descriptor_info[] =
			{
				Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[BASECOLOR_METALNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[previous_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[BASECOLOR_METALNESS].images[previous_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[previous_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[previous_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				//Descriptor_Info(0, framebuffer.render_targets[INDIRECT_DIFFUSE].images[0].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DENOISER_OUTPUT].images[previous_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[previous_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[INTERNAL_OCCLUSION_DATA].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_SPECULAR_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_SPECULAR_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DENOISER_SPECULAR_OUTPUT].images[previous_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			};

			struct
			{
				glm::ivec2 size;
				u32 frame_number;
				u32 frames_accumulated;
			} pc;

			pc.size = glm::ivec2(window_width, window_height);
			pc.frame_number = (u32)frame_counter;
			pc.frames_accumulated = frames_accumulated;
			vkCmdPushConstants(cmd, pipelines[TEMPORAL_ACCUMULATION].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[TEMPORAL_ACCUMULATION].update_template, pipelines[TEMPORAL_ACCUMULATION].layout, 0, descriptor_info);
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
		}

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			0,
			1, &memory_barrier,
			0, nullptr,
			0, nullptr
		);
		

#if 0
		{
			// History fix mip generation

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width / 2, window_height / 2, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[HISTORY_FIX_MIP_GEN].pipeline);


			Descriptor_Info descriptor_info[] =
			{
				//Descriptor_Info(0, framebuffer.render_targets[INDIRECT_DIFFUSE].images[0].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, history_fix.radiance_mip_views[0], VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, history_fix.radiance_mip_views[1], VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, history_fix.radiance_mip_views[2], VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, history_fix.radiance_mip_views[3], VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, history_fix.radiance_mip_views[4], VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, history_fix.view_z_mip_views[0], VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, history_fix.view_z_mip_views[1], VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, history_fix.view_z_mip_views[2], VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, history_fix.view_z_mip_views[3], VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, history_fix.view_z_mip_views[4], VK_IMAGE_LAYOUT_GENERAL),
			};

			struct
			{
				glm::ivec2 size;
				float depth_scale;
			} pc;

			pc.size = glm::ivec2(size.x, size.y);
			pc.depth_scale = scene.current_frame_camera.proj[3][2];
			vkCmdPushConstants(cmd, pipelines[HISTORY_FIX_MIP_GEN].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[HISTORY_FIX_MIP_GEN].update_template, pipelines[HISTORY_FIX_MIP_GEN].layout, 0, descriptor_info);
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
		}

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			0,
			1, &memory_barrier,
			0, nullptr,
			0, nullptr
		);
#endif

		if (g_settings.use_alternative_history_fix)
		{
			// Alternative history fix (sparse, wide spatial filter)

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[HISTORY_FIX_ALTERNATIVE].pipeline);

			struct
			{
				glm::ivec2 size;
				float stride;
			} pc;

			pc.size = glm::ivec2(size.x, size.y);
			pc.stride = g_settings.history_fix_stride;

			vkCmdPushConstants(cmd, pipelines[HISTORY_FIX_ALTERNATIVE].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			{
				Descriptor_Info descriptor_info[] =
				{
					Descriptor_Info(0, framebuffer.render_targets[DENOISER_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(0, framebuffer.render_targets[DENOISER_PING_PONG].images[PONG].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
					Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				};
				vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[HISTORY_FIX_ALTERNATIVE].update_template, pipelines[HISTORY_FIX_ALTERNATIVE].layout, 0, descriptor_info);

			}
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);

			{
				Descriptor_Info descriptor_info[] =
				{
					Descriptor_Info(0, framebuffer.render_targets[DENOISER_SPECULAR_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(0, framebuffer.render_targets[DENOISER_SPECULAR_PING_PONG].images[PONG].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
					Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
					Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				};
				vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[HISTORY_FIX_ALTERNATIVE].update_template, pipelines[HISTORY_FIX_ALTERNATIVE].layout, 0, descriptor_info);
			}

			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
		}
		else
		{
			// History fix

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[HISTORY_FIX].pipeline);

			Descriptor_Info descriptor_info[] =
			{
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], history_fix.radiance_image.image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], history_fix.view_z_image.image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			};

			struct
			{
				glm::ivec2 size;
			} pc;

			pc.size = glm::ivec2(size.x, size.y);
			vkCmdPushConstants(cmd, pipelines[HISTORY_FIX].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[HISTORY_FIX].update_template, pipelines[HISTORY_FIX].layout, 0, descriptor_info);
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
		}

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			0,
			1, &memory_barrier,
			0, nullptr,
			0, nullptr
		);

		{
			// Diffuse main blur pass

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[BLUR].pipeline);

			Descriptor_Info descriptor_info[] =
			{
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_PING_PONG].images[PONG].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DEBUG].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			};

			struct
			{
				glm::mat2 rot;
				glm::ivec2 size;
				float depth_scale;
				float blur_radius;
				float blur_radius_scale;
			} pc;

			float angle = 0.5f * (float)M_PI * van_der_corput_base_2[(frame_counter + 17) % 64];

			pc.size = glm::ivec2(window_width, window_height);
			pc.rot = glm::mat2(cosf(angle), sinf(angle), -sinf(angle), cosf(angle));
			pc.depth_scale = scene.current_frame_camera.proj[3][2];
			pc.blur_radius = g_settings.blur_radius;
			pc.blur_radius_scale = 1.0f;
			vkCmdPushConstants(cmd, pipelines[BLUR].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[BLUR].update_template, pipelines[BLUR].layout, 0, descriptor_info);
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
		}

		{
			// Specular main blur pass

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[BLUR_SPEC].pipeline);

			Descriptor_Info descriptor_info[] =
			{
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_SPECULAR_PING_PONG].images[PONG].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_SPECULAR_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DEBUG].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			};

			struct
			{
				glm::mat2 rot;
				glm::ivec2 size;
				float depth_scale;
				float blur_radius;
				float blur_radius_scale;
			} pc;

			float angle = 0.5f * (float)M_PI * van_der_corput_base_2[(frame_counter + 17) % 64];

			pc.size = glm::ivec2(window_width, window_height);
			pc.rot = glm::mat2(cosf(angle), sinf(angle), -sinf(angle), cosf(angle));
			pc.depth_scale = scene.current_frame_camera.proj[3][2];
			pc.blur_radius = g_settings.blur_radius;
			pc.blur_radius_scale = 1.0f;
			vkCmdPushConstants(cmd, pipelines[BLUR_SPEC].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[BLUR_SPEC].update_template, pipelines[BLUR_SPEC].layout, 0, descriptor_info);
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
		}

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			0,
			1, &memory_barrier,
			0, nullptr,
			0, nullptr
		);

		{
			// Diffuse post blur pass

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[POST_BLUR].pipeline);

			Descriptor_Info descriptor_info[] =
			{
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_OUTPUT].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DEBUG].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			};

			struct
			{
				glm::mat2 rot;
				glm::ivec2 size;
				float depth_scale;
				float blur_radius;
				float blur_radius_scale;
			} pc;

			float angle = 0.5f * (float)M_PI * van_der_corput_base_2[(frame_counter + 17) % 64];

			pc.size = glm::ivec2(window_width, window_height);
			pc.rot = glm::mat2(cosf(angle), sinf(angle), -sinf(angle), cosf(angle));
			pc.depth_scale = scene.current_frame_camera.proj[3][2];
			pc.blur_radius = g_settings.blur_radius;
			pc.blur_radius_scale = g_settings.post_blur_radius_scale;
			vkCmdPushConstants(cmd, pipelines[POST_BLUR].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[POST_BLUR].update_template, pipelines[POST_BLUR].layout, 0, descriptor_info);
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
		}

		{
			// Specular post blur pass

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[POST_BLUR_SPEC].pipeline);

			Descriptor_Info descriptor_info[] =
			{
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_SPECULAR_PING_PONG].images[PING].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[NORMAL_ROUGHNESS].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[DEPTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_SPECULAR_OUTPUT].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DEBUG].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			};

			struct
			{
				glm::mat2 rot;
				glm::ivec2 size;
				float depth_scale;
				float blur_radius;
				float blur_radius_scale;
			} pc;

			float angle = 0.5f * (float)M_PI * van_der_corput_base_2[(frame_counter + 17) % 64];

			pc.size = glm::ivec2(window_width, window_height);
			pc.rot = glm::mat2(cosf(angle), sinf(angle), -sinf(angle), cosf(angle));
			pc.depth_scale = scene.current_frame_camera.proj[3][2];
			pc.blur_radius = g_settings.blur_radius;
			pc.blur_radius_scale = g_settings.post_blur_radius_scale;
			vkCmdPushConstants(cmd, pipelines[POST_BLUR_SPEC].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[POST_BLUR_SPEC].update_template, pipelines[POST_BLUR_SPEC].layout, 0, descriptor_info);
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
		}

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			0,
			1, &memory_barrier,
			0, nullptr,
			0, nullptr
		);

		{
			// Diffuse temporal stabilization pass

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[TEMPORAL_STABILIZATION].pipeline);

			Descriptor_Info descriptor_info[] =
			{
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_OUTPUT].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[TEMPORAL_STABILIZATION_HISTORY].images[previous_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[TEMPORAL_STABILIZATION_HISTORY].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[INTERNAL_OCCLUSION_DATA].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			};

			struct
			{
				glm::ivec2 size;
			} pc;

			pc.size = glm::ivec2(window_width, window_height);
			vkCmdPushConstants(cmd, pipelines[POST_BLUR].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[TEMPORAL_STABILIZATION].update_template, pipelines[TEMPORAL_STABILIZATION].layout, 0, descriptor_info);
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
		}

		{
			// Specular temporal stabilization pass

			constexpr glm::uvec3 group_size = glm::uvec3(8, 8, 1);
			glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
			glm::uvec3 group_count = (size + (group_size - 1u)) & ~(group_size - 1u);
			group_count /= group_size;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[TEMPORAL_STABILIZATION].pipeline);

			Descriptor_Info descriptor_info[] =
			{
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_SPECULAR_OUTPUT].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(samplers[BILINEAR_SAMPLER_CLAMP], framebuffer.render_targets[TEMPORAL_STABILIZATION_HISTORY_SPEC].images[previous_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[TEMPORAL_STABILIZATION_HISTORY_SPEC].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[WORLD_POSITION].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(global_constants_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(gpu_camera_data.gpu_buffer.buffer, 0, VK_WHOLE_SIZE),
				Descriptor_Info(0, framebuffer.render_targets[DENOISER_HISTORY_LENGTH].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
				Descriptor_Info(0, framebuffer.render_targets[INTERNAL_OCCLUSION_DATA].images[current_frame_gbuffer_index].image_view, VK_IMAGE_LAYOUT_GENERAL),
			};

			struct
			{
				glm::ivec2 size;
			} pc;

			pc.size = glm::ivec2(window_width, window_height);
			vkCmdPushConstants(cmd, pipelines[POST_BLUR].layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipelines[TEMPORAL_STABILIZATION].update_template, pipelines[TEMPORAL_STABILIZATION].layout, 0, descriptor_info);
			vkCmdDispatch(cmd, group_count.x, group_count.y, 1);
		}
	}

	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		0,
		1, &memory_barrier,
		0, nullptr,
		0, nullptr
	);
}

void Renderer::cleanup()
{
	for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
		vkDestroyQueryPool(context->device, query_pools[i], nullptr);
	vkDestroyDescriptorUpdateTemplate(context->device, descriptor_update_template, nullptr);
	vkDestroyPipelineLayout(context->device, pipelines[PATH_TRACER_PIPELINE].layout, nullptr);
	vkDestroyPipeline(context->device, pipelines[PATH_TRACER_PIPELINE].pipeline, nullptr);
	vkDestroyDescriptorSetLayout(context->device, pipelines[PATH_TRACER_PIPELINE].desc_sets[0], nullptr);

	for (int i = 0; i < SAMPLER_COUNT; ++i)
	{
		if (samplers[i] != VK_NULL_HANDLE)
			vkDestroySampler(context->device, samplers[i], nullptr);
	}

	vmaUnmapMemory(context->allocator, global_constants_buffer.allocation);

	probe_system.shutdown();
	ui_overlay.shutdown();
}


void Renderer::transition_swapchain_images(VkCommandBuffer cmd)
{
	for (int i = 0; i < context->swapchain_images.size(); ++i)
	{
		vk_begin_command_buffer(cmd);
		vk_transition_layout(cmd, context->swapchain_images[i],
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_ACCESS_NONE, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		vkEndCommandBuffer(cmd);
		vk_command_buffer_single_submit(cmd);
		vkQueueWaitIdle(context->graphics_queue);
	}
}

static void vk_begin_command_buffer(VkCommandBuffer cmd)
{
	VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(cmd, &begin_info);
}

void Renderer::create_render_targets(VkCommandBuffer cmd)
{
	i32 w, h;
	platform->get_window_size(&w, &h);
	Render_Target color_attachments[2];
	for (size_t i = 0; i < std::size(color_attachments); ++i)
	{
		color_attachments[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		color_attachments[i].images[0] = context->allocate_image(
			{ (uint32_t)w, (uint32_t)h, 1 },
			color_attachments[i].format,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		);
	}
	Render_Target depth_attachment;
	depth_attachment.format = VK_FORMAT_D32_SFLOAT;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		depth_attachment.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			depth_attachment.format,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_ASPECT_DEPTH_BIT
		);
	}

	final_output = context->allocate_image(
		{ u32(w), (u32)h, 1 },
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
	);

	Render_Target normal_attachment;
	normal_attachment.format = NORMAL_ROUGHNESS_FORMAT;
	//normal_attachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		normal_attachment.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			normal_attachment.format,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target albedo_attachment;
	albedo_attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		albedo_attachment.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			albedo_attachment.format,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target world_pos_attachment;
	world_pos_attachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		world_pos_attachment.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			world_pos_attachment.format,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target indirect_diffuse_attachment;
	indirect_diffuse_attachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	indirect_diffuse_attachment.images[0] = context->allocate_image(
		{ (u32)w, (u32)h, 1 },
		indirect_diffuse_attachment.format,
		VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	Render_Target denoiser_output;
	denoiser_output.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		denoiser_output.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			denoiser_output.format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target denoiser_history_length;
	denoiser_history_length.format = VK_FORMAT_R8G8_UNORM;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		denoiser_history_length.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			denoiser_history_length.format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target denoiser_ping_pong;
	denoiser_ping_pong.format = denoiser_output.format;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		denoiser_ping_pong.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			denoiser_ping_pong.format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target denoiser_specular_ping_pong;
	denoiser_specular_ping_pong.format = denoiser_output.format;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		denoiser_specular_ping_pong.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			denoiser_specular_ping_pong.format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target debug;
	debug.format = VK_FORMAT_R8G8B8A8_UNORM;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		debug.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			debug.format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target temporal_stabilization;
	temporal_stabilization.format = denoiser_output.format;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		temporal_stabilization.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			temporal_stabilization.format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target temporal_stabilization_spec;
	temporal_stabilization_spec.format = denoiser_output.format;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		temporal_stabilization_spec.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			temporal_stabilization_spec.format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target occlusion_data;
	occlusion_data.format = VK_FORMAT_R8_UINT;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		occlusion_data.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			occlusion_data.format,
			VK_IMAGE_USAGE_STORAGE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target composition_output;
	composition_output.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		composition_output.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			composition_output.format,
			VK_IMAGE_USAGE_STORAGE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target taa_output;
	taa_output.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		taa_output.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			taa_output.format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target indirect_diffuse_sh;
	indirect_diffuse_sh.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		indirect_diffuse_sh.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			indirect_diffuse_sh.format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target indirect_specular;
	indirect_specular.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		indirect_specular.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			indirect_specular.format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	Render_Target denoiser_specular_output;
	denoiser_specular_output.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	for (u32 i = 0; i < GBUFFER_LAYERS; ++i)
	{
		denoiser_specular_output.images[i] = context->allocate_image(
			{ (u32)w, (u32)h, 1 },
			denoiser_specular_output.format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	history_fix.radiance_image = context->allocate_image(
		{ (u32)w, (u32)h, 1 },
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		HISTORY_FIX_MIP_LEVELS
	);

	history_fix.view_z_image = context->allocate_image(
		{ (u32)w, (u32)h, 1 },
		VK_FORMAT_R32_SFLOAT,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		HISTORY_FIX_MIP_LEVELS
	);

	for (int i = 0; i < HISTORY_FIX_MIP_LEVELS; ++i)
	{
		VkImageViewCreateInfo info = vkinit::image_view_create_info(history_fix.radiance_image.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, i, 1, 0, 1);
		vkCreateImageView(context->device, &info, nullptr, &history_fix.radiance_mip_views[i]);

		VkImageViewCreateInfo info2 = vkinit::image_view_create_info(history_fix.view_z_image.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_SFLOAT, i, 1, 0, 1);
		vkCreateImageView(context->device, &info2, nullptr, &history_fix.view_z_mip_views[i]);

		g_garbage_collector->push([=]()
			{
				vkDestroyImageView(context->device, history_fix.radiance_mip_views[i], nullptr);
				vkDestroyImageView(context->device, history_fix.view_z_mip_views[i], nullptr);
			}, Garbage_Collector::SHUTDOWN);
		};

	vk_begin_command_buffer(cmd);

	for (size_t i = 0; i < std::size(color_attachments); ++i)
	{
		vk_transition_layout(cmd, color_attachments[i].images[0].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, depth_attachment.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, 0,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			1, VK_IMAGE_ASPECT_DEPTH_BIT);
	}

	vk_transition_layout(cmd, final_output.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		0, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, normal_attachment.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, albedo_attachment.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}


	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, denoiser_output.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, world_pos_attachment.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, denoiser_history_length.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, denoiser_ping_pong.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, debug.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, temporal_stabilization.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, occlusion_data.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, composition_output.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, taa_output.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, indirect_diffuse_sh.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, indirect_specular.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, denoiser_specular_ping_pong.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, denoiser_specular_output.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < GBUFFER_LAYERS; ++i)
	{
		vk_transition_layout(cmd, temporal_stabilization_spec.images[i].image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}

	vk_transition_layout(cmd, indirect_diffuse_attachment.images[0].image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	vk_transition_layout(cmd, history_fix.radiance_image.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

	vk_transition_layout(cmd, history_fix.view_z_image.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

	vkEndCommandBuffer(cmd);
	vk_command_buffer_single_submit(cmd);
	vkQueueWaitIdle(context->graphics_queue);
	color_attachments[0].layout = VK_IMAGE_LAYOUT_GENERAL;
	color_attachments[0].name = "Color";
	framebuffer.render_targets[PATH_TRACER_COLOR] = color_attachments[0];
	framebuffer.render_targets[RASTER_COLOR] = color_attachments[1];
	framebuffer.render_targets[DEPTH] = depth_attachment;
	framebuffer.render_targets[NORMAL_ROUGHNESS] = normal_attachment;
	framebuffer.render_targets[BASECOLOR_METALNESS] = albedo_attachment;
	framebuffer.render_targets[INDIRECT_DIFFUSE] = indirect_diffuse_attachment;
	framebuffer.render_targets[INDIRECT_DIFFUSE_SH] = indirect_diffuse_sh;
	framebuffer.render_targets[INDIRECT_SPECULAR] = indirect_specular;
	framebuffer.render_targets[DENOISER_OUTPUT] = denoiser_output;
	framebuffer.render_targets[DENOISER_SPECULAR_OUTPUT] = denoiser_specular_output;
	framebuffer.render_targets[WORLD_POSITION] = world_pos_attachment;
	framebuffer.render_targets[DENOISER_HISTORY_LENGTH] = denoiser_history_length;
	framebuffer.render_targets[DENOISER_PING_PONG] = denoiser_ping_pong;
	framebuffer.render_targets[DENOISER_SPECULAR_PING_PONG] = denoiser_specular_ping_pong;
	framebuffer.render_targets[DEBUG] = debug;
	framebuffer.render_targets[TEMPORAL_STABILIZATION_HISTORY] = temporal_stabilization;
	framebuffer.render_targets[TEMPORAL_STABILIZATION_HISTORY_SPEC] = temporal_stabilization_spec;
	framebuffer.render_targets[INTERNAL_OCCLUSION_DATA] = occlusion_data;
	framebuffer.render_targets[COMPOSITION_OUTPUT] = composition_output;
	framebuffer.render_targets[TAA_OUTPUT] = taa_output;
}

static bool check_extensions(const std::vector<const char*>& device_exts, const std::vector<VkExtensionProperties>& props)
{
	for (const auto& ext : device_exts)
	{
		bool found = false;
		for (const auto& prop : props)
		{
			if (!strcmp(ext, prop.extensionName))
			{
				found = true;
				break;
			}
		}
		if (!found) return false;
	}
	return true;
}


void Renderer::create_bottom_level_acceleration_structure(Mesh* mesh)
{
	u32 prim_count = (u32)mesh->primitives.size();

	for (u32 i = 0; i < prim_count; ++i)
	{
		assert(!mesh->primitives[i].acceleration_structure.has_value());

		VkAccelerationStructureBuildGeometryInfoKHR build_info{};
		VkAccelerationStructureGeometryKHR geometry{};
		VkAccelerationStructureBuildRangeInfoKHR range_info{};
		VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangles.vertexData.deviceAddress = mesh->vertex_buffer_address;
		triangles.vertexStride = mesh->get_vertex_size();
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.indexData.deviceAddress = mesh->index_buffer_address;
		triangles.maxVertex = mesh->get_vertex_count() - 1;
		triangles.transformData = { 0 };

		geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.geometry.triangles = triangles;
		geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

		range_info = {};
		range_info.firstVertex = 0;
		range_info.primitiveCount = mesh->primitives[i].vertex_count / 3;
		range_info.primitiveOffset = mesh->primitives[i].vertex_offset * sizeof(u32);
		range_info.transformOffset = 0;

		build_info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		build_info.geometryCount = 1;
		build_info.pGeometries = &geometry;
		build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		build_info.srcAccelerationStructure = VK_NULL_HANDLE;


		VkAccelerationStructureBuildSizesInfoKHR size_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		vkGetAccelerationStructureBuildSizesKHR(context->device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, &range_info.primitiveCount, &size_info);

		Vk_Allocated_Buffer buffer_blas = context->allocate_buffer((uint32_t)size_info.accelerationStructureSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

		VkAccelerationStructureKHR acceleration_structure;
		VkAccelerationStructureCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
		create_info.type = build_info.type;
		create_info.size = size_info.accelerationStructureSize;
		create_info.buffer = buffer_blas.buffer;
		create_info.offset = 0;
		VK_CHECK(vkCreateAccelerationStructureKHR(context->device, &create_info, nullptr, &acceleration_structure));

		g_garbage_collector->push([=]()
			{
				vkDestroyAccelerationStructureKHR(context->device, acceleration_structure, nullptr);
			}, Garbage_Collector::SHUTDOWN);

		Vk_Allocated_Buffer scratch_buffer = context->allocate_buffer((uint32_t)size_info.buildScratchSize,
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0, context->acceleration_structure_properties.minAccelerationStructureScratchOffsetAlignment);


		Acceleration_Structure as{};
		as.level = Acceleration_Structure::Level::BOTTOM_LEVEL;
		as.acceleration_structure = acceleration_structure;
		as.scratch_buffer = scratch_buffer;
		as.scratch_buffer_address = context->get_buffer_device_address(scratch_buffer);
		as.acceleration_structure_buffer = buffer_blas;
		as.acceleration_structure_buffer_address = context->get_acceleration_structure_device_address(acceleration_structure);

		mesh->primitives[i].acceleration_structure = as;
	};


}


void Renderer::build_bottom_level_acceleration_structure(Mesh* mesh, VkCommandBuffer cmd)
{
	u32 prim_count = (u32)mesh->primitives.size();

	for (u32 i = 0; i < prim_count; ++i)
	{
		VkAccelerationStructureBuildGeometryInfoKHR build_info{};
		VkAccelerationStructureGeometryKHR geometry{};
		VkAccelerationStructureBuildRangeInfoKHR range_info{};
		VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangles.vertexData.deviceAddress = mesh->vertex_buffer_address;
		triangles.vertexStride = mesh->get_vertex_size();
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.indexData.deviceAddress = mesh->index_buffer_address;
		triangles.maxVertex = mesh->get_vertex_count() - 1;
		triangles.transformData = { 0 };

		geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.geometry.triangles = triangles;
		geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

		range_info = {};
		range_info.firstVertex = 0;
		range_info.primitiveCount = mesh->primitives[i].vertex_count / 3;
		range_info.primitiveOffset = mesh->primitives[i].vertex_offset * sizeof(u32);
		range_info.transformOffset = 0;

		build_info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		build_info.geometryCount = 1;
		build_info.pGeometries = &geometry;
		build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		build_info.srcAccelerationStructure = VK_NULL_HANDLE;

		build_info.dstAccelerationStructure = mesh->primitives[i].acceleration_structure.value().acceleration_structure;
		build_info.scratchData.deviceAddress = mesh->primitives[i].acceleration_structure.value().scratch_buffer_address;

		VkAccelerationStructureBuildRangeInfoKHR* p_range_info = &range_info;
		vkCmdBuildAccelerationStructuresKHR(
			cmd,
			1,
			&build_info,
			&p_range_info
		);
	}
}

void Renderer::create_top_level_acceleration_structure(ECS* ecs, VkCommandBuffer cmd)
{
	std::vector<VkAccelerationStructureInstanceKHR> instances;

	u32 index = 0;
	for (auto [mesh, xform] : ecs->filter<Static_Mesh_Component, Transform_Component>())
	{
		Mesh* m = mesh->manager->get_resource_with_id(mesh->mesh_id);

		glm::mat4 rot = glm::toMat4(xform->rotation);
		glm::mat4 trans = glm::translate(glm::mat4(1.0), glm::vec3(xform->pos));
		glm::mat4 scale = glm::scale(glm::mat4(1.0), glm::vec3(xform->scale));
		//glm::mat4 scale = glm::scale(glm::mat4(1.0), glm::vec3(0.01f));
		glm::mat4 transform = trans * rot * scale;

		u32 prim_count = (u32)m->primitives.size();
		for (u32 i = 0; i < prim_count; ++i)
		{
			VkAccelerationStructureInstanceKHR instance{};
			for (int i = 0; i < 4; ++i)
				for (int j = 0; j < 3; ++j)
				{
					instance.transform.matrix[j][i] = transform[i][j];
				}

			//i32 mat_id = m->primitives[i].material_id;
			i32 mat_id = i;
			i32 mesh_id = mesh->mesh_id;

			instance.instanceCustomIndex = ((mat_id & 0x3FF) << 14) | (mesh_id & 0x3FFF);
			instance.mask = 0xFF;
			instance.instanceShaderBindingTableRecordOffset = 0;
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			instance.accelerationStructureReference = m->primitives[i].acceleration_structure.value().acceleration_structure_buffer_address;

			instances.push_back(instance);
		}
	}

	size_t size = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();
	GPU_Buffer instance_buf = context->create_gpu_buffer(
		(u32)size,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		16
	);

	instance_buf.update_staging_buffer(context->allocator, current_frame_index, instances.data(), size);
	instance_buf.upload(cmd, current_frame_index);

	vkinit::memory_barrier(cmd,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

	VkAccelerationStructureBuildRangeInfoKHR range_info{};
	range_info.primitiveOffset = 0;
	range_info.primitiveCount = (u32)instances.size();
	range_info.firstVertex = 0;
	range_info.transformOffset = 0;

	VkAccelerationStructureGeometryInstancesDataKHR instances_vk{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR
	};
	instances_vk.arrayOfPointers = VK_FALSE;
	instances_vk.data.deviceAddress = context->get_buffer_device_address(instance_buf.gpu_buffer);

	VkAccelerationStructureGeometryKHR geometry{ 
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR 
	};

	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances = instances_vk;

	VkAccelerationStructureBuildGeometryInfoKHR build_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR
	};
	build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	build_info.geometryCount = 1;
	build_info.pGeometries = &geometry;
	build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	build_info.srcAccelerationStructure = VK_NULL_HANDLE;

	VkAccelerationStructureBuildSizesInfoKHR size_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};
	vkGetAccelerationStructureBuildSizesKHR(
		context->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&build_info,
		&range_info.primitiveCount,
		&size_info
	);

	Vk_Allocated_Buffer buffer_tlas = context->allocate_buffer(
		(u32)size_info.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0
	);


	VkAccelerationStructureCreateInfoKHR create_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR
	};
	create_info.type = build_info.type;
	create_info.size = size_info.accelerationStructureSize;
	create_info.buffer = buffer_tlas.buffer;
	create_info.offset = 0;
	VkAccelerationStructureKHR tlas;
	VK_CHECK(vkCreateAccelerationStructureKHR(
		context->device, &create_info, nullptr, &tlas
	));

	g_garbage_collector->push([=]()
		{
			vkDestroyAccelerationStructureKHR(context->device, tlas, nullptr);
		}, Garbage_Collector::SHUTDOWN);

	build_info.dstAccelerationStructure = tlas;

	Vk_Allocated_Buffer scratch = context->allocate_buffer(
		(uint32_t)size_info.buildScratchSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0
	);


	build_info.scratchData.deviceAddress = context->get_buffer_device_address(scratch);

	VkAccelerationStructureBuildRangeInfoKHR* p_range_info = &range_info;
	
	vkCmdBuildAccelerationStructuresKHR(
		cmd,
		1, &build_info,
		&p_range_info
	);

	Acceleration_Structure scene_tlas{};
	scene_tlas.acceleration_structure = tlas;
	scene_tlas.acceleration_structure_buffer = buffer_tlas;
	scene_tlas.acceleration_structure_buffer_address = context->get_buffer_device_address(buffer_tlas);
	scene_tlas.scratch_buffer = scratch;
	scene_tlas.scratch_buffer_address = context->get_buffer_device_address(scratch);
	scene_tlas.level = Acceleration_Structure::TOP_LEVEL;
	//scene_tlas.tlas_instances = instance_buf;
	scene_tlas.tlas_instances_address = context->get_buffer_device_address(instance_buf.gpu_buffer);

	assert(!scene.tlas.has_value());
	scene.tlas = scene_tlas;
}

Vk_Allocated_Image Renderer::prefilter_envmap(VkCommandBuffer cmd, Vk_Allocated_Image envmap)
{
	constexpr int mip_levels = 5;
	VkExtent3D image_extent{};
	image_extent.width = 4096;
	image_extent.height = 2048;
	image_extent.depth = 1;

	Vk_Allocated_Image prefiltered = context->allocate_image(image_extent,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_TILING_OPTIMAL, mip_levels
	);


	vk_transition_layout(cmd, prefiltered.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, mip_levels
	);
#if PREFILTER_ENVMAP
	Vk_Pipeline prefilter_envmap_pp = context->create_compute_pipeline("shaders/spirv/prefilter_envmap.comp.spv");
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, prefilter_envmap_pp.pipeline);

	float roughness_step = 1.0f / float(mip_levels - 1);
	glm::uvec2 size = glm::uvec2(4096, 2048);
	for (int i = 0; i < mip_levels; ++i)
	{
		VkImageViewCreateInfo cinfo = vkinit::image_view_create_info(
			prefiltered.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, i 
		);
		VkImageView v;
		vkCreateImageView(context->device, &cinfo, nullptr, &v);

		g_garbage_collector->push([=]()
		{
			vkDestroyImageView(context->device, v, nullptr);
		}, Garbage_Collector::END_OF_FRAME);

		Descriptor_Info info[] = {
			Descriptor_Info(bilinear_sampler, environment_map.image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			Descriptor_Info(0, v, VK_IMAGE_LAYOUT_GENERAL),
		};

		vkCmdPushDescriptorSetWithTemplateKHR(cmd, prefilter_envmap_pp.update_template, prefilter_envmap_pp.layout, 0, info);
		struct
		{
			float roughness;
		} pc;

		pc.roughness = (float)i * roughness_step;

		vkCmdPushConstants(cmd, prefilter_envmap_pp.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

		glm::uvec2 group_size = size / 8u;
		assert(size % 8u == glm::uvec2(0));
		size /= 2u;
		vkCmdDispatch(cmd, group_size.x, group_size.y, 1);
	}
#endif
	vk_transition_layout(cmd, prefiltered.image,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_SHADER_WRITE_BIT, 0,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, mip_levels
	);


	return prefiltered;
}

void Renderer::change_render_mode(Rendering_Mode new_mode)
{
	frames_accumulated = 0;
	needs_history_clear = true;
}

void Renderer::create_lookup_textures()
{
	Vk_Pipeline pipeline = context->create_compute_pipeline("shaders/spirv/create_brdf_lut.comp.spv");
	
	VkExtent3D image_extent{};
	image_extent.width = 512;
	image_extent.height = 512;
	image_extent.depth = 1;
	brdf_lut = context->allocate_image(image_extent, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

	Descriptor_Info info[] = {
		{0, brdf_lut.image_view, VK_IMAGE_LAYOUT_GENERAL}
	};

	VkCommandBuffer cmd = get_current_frame_command_buffer();
	vk_begin_command_buffer(cmd);

	vkinit::vk_transition_layout(
		cmd, brdf_lut.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
	);

#if CREATE_BRDF_LUT

	glm::uvec3 group_count = glm::uvec3(512 / 8, 512 / 8, 1);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);
	vkCmdPushDescriptorSetWithTemplateKHR(cmd, pipeline.update_template, pipeline.layout, 0, info);
	glm::uvec2 size = glm::uvec2(512, 512);
	vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(size), &size);
	vkCmdDispatch(cmd, group_count.x, group_count.y, group_count.z);
#endif
	vkinit::vk_transition_layout(
		cmd, brdf_lut.image,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
	);

#
	vkEndCommandBuffer(cmd);

	vk_command_buffer_single_submit(cmd);
	vkQueueWaitIdle(context->graphics_queue);
}
