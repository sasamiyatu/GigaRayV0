#include "renderer.h"
#include "common.h"
#include "vk_helpers.h"

#include <assert.h>
#include <array>
#include <r_mesh.h>
#include <ecs.h>

using namespace vkinit;

static void vk_begin_command_buffer(VkCommandBuffer cmd);


// FIXME: We're gonna need more stuff than this 
void Renderer::vk_create_descriptor_set_layout()
{
	std::array<VkDescriptorSetLayoutBinding, 6> bindings{};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;


	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;


	VkDescriptorSetLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_info.bindingCount = uint32_t(bindings.size());
	layout_info.pBindings = bindings.data();
	layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	VK_CHECK(vkCreateDescriptorSetLayout(context->device, &layout_info, nullptr, &desc_set_layout));

	g_garbage_collector->push([=]()
		{
			vkDestroyDescriptorSetLayout(context->device, desc_set_layout, nullptr);
		}, Garbage_Collector::SHUTDOWN);

	// NOTE: No descriptor pools because we are using push descriptors for now!!
}


// FIXME: Hardcoded shaders and hitgroups
Vk_Pipeline Renderer::vk_create_rt_pipeline()
{
	VkShaderModule rgen_shader = context->create_shader_module_from_file("shaders/spirv/test.rgen.spv");
	VkShaderModule rmiss_shader = context->create_shader_module_from_file("shaders/spirv/test.rmiss.spv");
	VkShaderModule rchit_shader = context->create_shader_module_from_file("shaders/spirv/test.rchit.spv");

	constexpr int n_rt_shader_stages = 3;
	constexpr int n_rt_shader_groups = 3;

	constexpr int raygen_index = 0;
	constexpr int miss_index = 1;
	constexpr int closest_hit_index = 2;

	constexpr int raygen_group_index = 0;
	constexpr int miss_group_index = 1;
	constexpr int hit_group_index = 2;

	std::array<VkPipelineShaderStageCreateInfo, n_rt_shader_stages> pssci{ };

	pssci[raygen_index].sType = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	pssci[raygen_index].module = rgen_shader;
	pssci[raygen_index].pName = "main";
	pssci[raygen_index].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	pssci[miss_index].sType = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	pssci[miss_index].module = rmiss_shader;
	pssci[miss_index].pName = "main";
	pssci[miss_index].stage = VK_SHADER_STAGE_MISS_BIT_KHR;

	pssci[closest_hit_index].sType = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	pssci[closest_hit_index].module = rchit_shader;
	pssci[closest_hit_index].pName = "main";
	pssci[closest_hit_index].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	std::array<VkRayTracingShaderGroupCreateInfoKHR, n_rt_shader_groups> rtsgci{};
	rtsgci[raygen_group_index].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	rtsgci[raygen_group_index].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	rtsgci[raygen_group_index].generalShader = raygen_index;
	rtsgci[raygen_group_index].closestHitShader = VK_SHADER_UNUSED_KHR;
	rtsgci[raygen_group_index].anyHitShader = VK_SHADER_UNUSED_KHR;
	rtsgci[raygen_group_index].intersectionShader = VK_SHADER_UNUSED_KHR;

	rtsgci[miss_group_index] = rtsgci[raygen_group_index];
	rtsgci[miss_group_index].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	rtsgci[miss_group_index].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	rtsgci[miss_group_index].generalShader = miss_index;
	rtsgci[miss_group_index].closestHitShader = VK_SHADER_UNUSED_KHR;
	rtsgci[miss_group_index].anyHitShader = VK_SHADER_UNUSED_KHR;
	rtsgci[miss_group_index].intersectionShader = VK_SHADER_UNUSED_KHR;

	rtsgci[hit_group_index].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	rtsgci[hit_group_index].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	rtsgci[hit_group_index].generalShader = VK_SHADER_UNUSED_KHR;
	rtsgci[hit_group_index].closestHitShader = closest_hit_index;
	rtsgci[hit_group_index].anyHitShader = VK_SHADER_UNUSED_KHR;
	rtsgci[hit_group_index].intersectionShader = VK_SHADER_UNUSED_KHR;

	// Create pipeline layout
	VkPipelineLayoutCreateInfo cinfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	cinfo.pSetLayouts = &desc_set_layout;
	cinfo.setLayoutCount = 1;
	VkPushConstantRange push_constant_range{};
	push_constant_range.offset = 0;
	push_constant_range.size = sizeof(u32);
	push_constant_range.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	cinfo.pushConstantRangeCount = 1;
	cinfo.pPushConstantRanges = &push_constant_range;
	VkPipelineLayout layout;
	vkCreatePipelineLayout(context->device, &cinfo, nullptr, &layout);
	g_garbage_collector->push([=]()
		{
			vkDestroyPipelineLayout(context->device, layout, nullptr);
		}, Garbage_Collector::SHUTDOWN);

	VkRayTracingPipelineCreateInfoKHR rtpci{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	rtpci.stageCount = (uint32_t)pssci.size();
	rtpci.pStages = pssci.data();
	rtpci.groupCount = (uint32_t)rtsgci.size();
	rtpci.pGroups = rtsgci.data();
	rtpci.maxPipelineRayRecursionDepth = 4;
	rtpci.layout = layout;

	VkPipeline rt_pipeline;
	VK_CHECK(vkCreateRayTracingPipelinesKHR(
		context->device,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		1, &rtpci,
		nullptr,
		&rt_pipeline));

	g_garbage_collector->push([=]()
		{
			vkDestroyPipeline(context->device, rt_pipeline, nullptr);
		}, Garbage_Collector::SHUTDOWN);

	Vk_Pipeline pl;
	pl.layout = layout;
	pl.pipeline = rt_pipeline;

	uint32_t group_count = (uint32_t)rtsgci.size();
	// TODO: Compute these elsewhere as these are device properties, should be part of Vk_Context
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	rt_pipeline_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	VkPhysicalDeviceProperties2 device_props{};
	device_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	device_props.pNext = &rt_pipeline_props;
	vkGetPhysicalDeviceProperties2(context->physical_device, &device_props);
	uint32_t group_handle_size = rt_pipeline_props.shaderGroupHandleSize;
	uint32_t group_size_aligned = (group_handle_size + rt_pipeline_props.shaderGroupBaseAlignment - 1) & ~(rt_pipeline_props.shaderGroupBaseAlignment - 1);

	uint32_t sbt_size = group_count * group_size_aligned;

	std::vector<uint8_t> shader_handle_storage(sbt_size);
	VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(
		context->device,
		rt_pipeline,
		0,
		group_count,
		sbt_size,
		shader_handle_storage.data()
	));

	Vk_Allocated_Buffer rt_sbt_buffer = context->allocate_buffer(sbt_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
	);

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(context->allocator, rt_sbt_buffer.buffer, rt_sbt_buffer.allocation);
		}, Garbage_Collector::SHUTDOWN);

	void* mapped;
	vmaMapMemory(context->allocator, rt_sbt_buffer.allocation, &mapped);
	uint8_t* pdata = (uint8_t*)mapped;
	for (uint32_t g = 0; g < group_count; ++g)
	{
		memcpy(pdata, shader_handle_storage.data() + g * group_handle_size,
			group_handle_size);
		pdata += group_size_aligned;
	}
	vmaUnmapMemory(context->allocator, rt_sbt_buffer.allocation);

	shader_binding_table = rt_sbt_buffer;
	context->device_sbt_alignment = group_size_aligned;

	vkDestroyShaderModule(context->device, rgen_shader, nullptr);
	vkDestroyShaderModule(context->device, rmiss_shader, nullptr);
	vkDestroyShaderModule(context->device, rchit_shader, nullptr);

	return pl;
}

void Renderer::create_samplers()
{
	VkSamplerCreateInfo cinfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	cinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	cinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	cinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	cinfo.minFilter = VK_FILTER_LINEAR;
	cinfo.magFilter = VK_FILTER_LINEAR;
	vkCreateSampler(context->device, &cinfo, nullptr, &bilinear_sampler);

	g_garbage_collector->push([=]()
		{
			vkDestroySampler(context->device, bilinear_sampler, nullptr);
		}, Garbage_Collector::SHUTDOWN);
}


Renderer::Renderer(Vk_Context* context, Platform* platform, Resource_Manager<Mesh>* mesh_manager, Resource_Manager<Texture>* texture_manager)
	: context(context),
	platform(platform),
	rt_pipeline({}),
	mesh_manager(mesh_manager),
	texture_manager(texture_manager)
{
	initialize();
}



void Renderer::initialize()
{
	vk_create_descriptor_set_layout();

	rt_pipeline = vk_create_rt_pipeline();
	transition_swapchain_images(get_current_frame_command_buffer());
	vk_create_render_targets(get_current_frame_command_buffer());
	compute_pp = context->create_compute_pipeline("shaders/spirv/test.comp.spv");
	platform->get_window_size(&window_width, &window_height);
	create_samplers();
	initialized = true;
}

VkCommandBuffer Renderer::get_current_frame_command_buffer()
{
	uint32_t frame_index = get_frame_index();
	return context->frame_objects[frame_index].cmd;
}

void Renderer::vk_command_buffer_single_submit(VkCommandBuffer cmd)
{
	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd;

	vkQueueSubmit(context->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
}

// Loads the meshes from the scene and creates the acceleration structures
void Renderer::init_scene(ECS* ecs)
{
	environment_map = context->load_texture_hdri("data/kloppenheim_06_puresky_4k.hdr");
	g_garbage_collector->push([=]()
		{
			vmaDestroyImage(context->allocator, environment_map.image, environment_map.allocation);
		}, Garbage_Collector::SHUTDOWN);
	// Find first active camera
	for (auto [cam] : ecs->filter<Camera_Component>())
	{
		scene.active_camera = cam;
		break;
	}

	u32 aligned_size = vkinit::aligned_size(
		sizeof(Camera_Component), 
		(u32)context->physical_device_properties.properties.limits.minUniformBufferOffsetAlignment
	);
	gpu_camera_data = context->allocate_buffer(aligned_size * FRAMES_IN_FLIGHT,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(context->allocator, gpu_camera_data.buffer, gpu_camera_data.allocation);
		}, Garbage_Collector::SHUTDOWN);

	VkCommandBuffer cmd = get_current_frame_command_buffer();
	vk_begin_command_buffer(cmd);
	for (auto [mesh] : ecs->filter<Static_Mesh_Component>())
	{
		Mesh* m = mesh->manager->get_resource_with_id(mesh->mesh_id);
		create_vertex_buffer(m, cmd);
		create_index_buffer(m, cmd);
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

	vkQueueWaitIdle(context->graphics_queue);
}

void Renderer::pre_frame()
{
	u32 frame_index = frame_counter % FRAMES_IN_FLIGHT;
	if (scene.active_camera->dirty)
	{
		frames_accumulated = 0;
		scene.active_camera->dirty = false;
	}
	void* camera_data;
	vmaMapMemory(context->allocator, gpu_camera_data.allocation, &camera_data);
	u32 aligned_size = vkinit::aligned_size(sizeof(Camera_Component),
		(u32)context->physical_device_properties.properties.limits.minUniformBufferOffsetAlignment
	);
	u8* write_address = (u8*)camera_data + aligned_size * frame_index;
	memcpy(write_address, scene.active_camera, sizeof(Camera_Component));
	vmaUnmapMemory(context->allocator, gpu_camera_data.allocation);
}

void Renderer::begin_frame()
{
	uint32_t frame_index = get_frame_index();
	vkWaitForFences(context->device, 1, &context->frame_objects[frame_index].fence, VK_TRUE, UINT64_MAX);
	vkResetFences(context->device, 1, &context->frame_objects[frame_index].fence);
	vkAcquireNextImageKHR(context->device, context->swapchain,
		UINT64_MAX, context->frame_objects[frame_index].image_available_sem,
		VK_NULL_HANDLE, &swapchain_image_index);

	VkCommandBuffer cmd = get_current_frame_command_buffer();
	VkImage next_image = context->swapchain_images[swapchain_image_index];

	vk_begin_command_buffer(cmd);

	vk_transition_layout(cmd, next_image,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
	);
}

void Renderer::end_frame()
{
	uint32_t frame_index = get_frame_index();

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
	region.srcOffsets[1] = { 1280, 720, 1 };
	region.dstOffsets[1] = { 1280, 720, 1 };
	VkImageSubresourceLayers dst_layer{};
	dst_layer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	dst_layer.baseArrayLayer = 0;
	dst_layer.mipLevel = 0;
	dst_layer.layerCount = 1;
	region.dstSubresource = dst_layer;

	VkImage next_image = context->swapchain_images[swapchain_image_index];
	vkCmdBlitImage(cmd, 
		final_output.image, VK_IMAGE_LAYOUT_GENERAL, next_image, VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_LINEAR);
	vk_transition_layout(
		cmd, next_image,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		0, 0,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
	);

	VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;
	submit.signalSemaphoreCount = 1;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &context->frame_objects[frame_index].image_available_sem;
	submit.pSignalSemaphores = &context->frame_objects[frame_index].render_finished_sem;

	vkEndCommandBuffer(cmd);

	vkQueueSubmit(context->graphics_queue, 1, &submit, context->frame_objects[frame_index].fence);

	VkPresentInfoKHR present_info{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &context->swapchain;
	present_info.pImageIndices = &swapchain_image_index;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &context->frame_objects[frame_index].render_finished_sem;
	vkQueuePresentKHR(context->graphics_queue, &present_info);

	frame_counter++;
}

void Renderer::draw(ECS* ecs)
{
	VkCommandBuffer cmd = get_current_frame_command_buffer();

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline.pipeline);

	// Update descriptor set

	VkWriteDescriptorSet writes[6] = { };
	for (int i = 0; i < std::size(writes); ++i)
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[0].dstArrayElement = 0;
	writes[0].dstBinding = 0;
	VkDescriptorImageInfo img_info{};
	img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	img_info.imageView = framebuffer.render_targets[0].image.image_view;
	writes[0].pImageInfo = &img_info;
	writes[0].dstSet = 0;

	VkWriteDescriptorSetAccelerationStructureKHR acr{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	acr.accelerationStructureCount = 1;
	acr.pAccelerationStructures = &scene.tlas.value().acceleration_structure;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writes[1].dstBinding = 1;
	writes[1].dstSet = 0;
	writes[1].pNext = &acr;

	Mesh* m = get_mesh(ecs, 0);
	// Vertex buffer
	VkDescriptorBufferInfo buffer_info[2] = {};
	buffer_info[0].buffer = m->vertex_buffer.buffer;
	buffer_info[0].offset = 0;
	buffer_info[0].range = VK_WHOLE_SIZE;
	buffer_info[1].buffer = m->index_buffer.buffer;
	buffer_info[1].offset = 0;
	buffer_info[1].range = VK_WHOLE_SIZE;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].dstBinding = 2;
	writes[2].dstSet = 0;
	writes[2].pBufferInfo = &buffer_info[0];

	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[3].dstBinding = 3;
	writes[3].dstSet = 0;
	writes[3].pBufferInfo = &buffer_info[1];

	u32 frame_index = frame_counter % FRAMES_IN_FLIGHT;
	VkDescriptorBufferInfo camera_buf;
	u32 aligned_size = vkinit::aligned_size(
		sizeof(Camera_Component),
		(u32)context->physical_device_properties.properties.limits.minUniformBufferOffsetAlignment
	);
	camera_buf.buffer = gpu_camera_data.buffer;
	camera_buf.offset = aligned_size * frame_index;
	camera_buf.range = aligned_size;
	writes[4].descriptorCount = 1;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[4].dstBinding = 4;
	writes[4].dstSet = 0;
	writes[4].pBufferInfo = &camera_buf;

	VkDescriptorImageInfo envmap_img{};
	envmap_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	envmap_img.imageView = environment_map.image_view;
	envmap_img.sampler = bilinear_sampler;
	writes[5].descriptorCount = 1;
	writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[5].dstBinding = 5;
	writes[5].dstSet = 0;
	writes[5].pImageInfo = &envmap_img;

	vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline.layout, 0, (uint32_t)std::size(writes), writes);

	vkCmdPushConstants(cmd, rt_pipeline.layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(frames_accumulated), &frames_accumulated);

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
		1280, 720, 1
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

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pp.pipeline);
	// Go for groupsize 8x8?
	constexpr u32 group_size = 8;
	glm::uvec3 size = glm::uvec3(window_width, window_height, 1);
	glm::uvec3 group_count = (size + (group_size - 1)) & ~(group_size - 1);
	group_count /= group_size;
	
	glm::ivec2 p = glm::ivec2(size.x, size.y);

	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].dstArrayElement = 0;
	writes[1].dstBinding = 1;
	VkDescriptorImageInfo img_info2 = {};
	img_info2.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	img_info2.imageView = final_output.image_view;
	writes[1].pImageInfo = &img_info2;
	writes[1].dstSet = 0;
	vkCmdPushConstants(cmd, compute_pp.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(p), &p);
	vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pp.layout, 0, 2, &writes[0]);
	vkCmdDispatch(cmd, group_count.x, group_count.y, group_count.z);
	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		0, nullptr);

	frames_accumulated++;
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

void Renderer::vk_create_render_targets(VkCommandBuffer cmd)
{
	framebuffer.render_targets.resize(1);
	i32 w, h;
	platform->get_window_size(&w, &h);
	Vk_RenderTarget color_attachment;
	color_attachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	color_attachment.image = context->allocate_image(
		{ (uint32_t)w, (uint32_t)h, 1 }, 
		color_attachment.format, 
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
	);
	g_garbage_collector->push([=]()
		{
			vmaDestroyImage(context->allocator, color_attachment.image.image, color_attachment.image.allocation);
		}, Garbage_Collector::SHUTDOWN);


	final_output = context->allocate_image(
		{ u32(w), (u32)h, 1 },
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT
	);
	g_garbage_collector->push([=]()
		{
			vmaDestroyImage(context->allocator, final_output.image, final_output.allocation);
		}, Garbage_Collector::SHUTDOWN);


	vk_begin_command_buffer(cmd);
	vk_transition_layout(cmd, color_attachment.image.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

	vk_transition_layout(cmd, final_output.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	vkEndCommandBuffer(cmd);
	vk_command_buffer_single_submit(cmd);
	vkQueueWaitIdle(context->graphics_queue);
	color_attachment.layout = VK_IMAGE_LAYOUT_GENERAL;
	color_attachment.name = "Color";
	framebuffer.render_targets[0] = color_attachment;
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

uint32_t Mesh::get_vertex_buffer_size()
{
	return (uint32_t)(sizeof(vertices[0]) * vertices.size());
}

uint32_t Mesh::get_index_buffer_size()
{
	return (uint32_t)(sizeof(uint32_t) * indices.size());
}


void Renderer::create_vertex_buffer(Mesh* mesh, VkCommandBuffer cmd)
{
	u32 buffer_size = mesh->get_vertex_buffer_size();
	mesh->vertex_buffer = context->allocate_buffer(buffer_size,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
		| VK_BUFFER_USAGE_TRANSFER_DST_BIT
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(context->allocator, mesh->vertex_buffer.buffer, mesh->vertex_buffer.allocation);
		}, Garbage_Collector::SHUTDOWN);

	mesh->vertex_buffer_address = context->get_buffer_device_address(mesh->vertex_buffer);

	Vk_Allocated_Buffer tmp_staging_buffer = context->allocate_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 0);

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(context->allocator, tmp_staging_buffer.buffer, tmp_staging_buffer.allocation);
		}, Garbage_Collector::SHUTDOWN);

	void* mapped;
	vmaMapMemory(context->allocator, tmp_staging_buffer.allocation, &mapped);
	memcpy(mapped, mesh->vertices.data(), buffer_size);
	vmaUnmapMemory(context->allocator, tmp_staging_buffer.allocation);

	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0; // Optional
	copy_region.dstOffset = 0; // Optional
	copy_region.size = buffer_size;

	vkCmdCopyBuffer(cmd, tmp_staging_buffer.buffer, mesh->vertex_buffer.buffer, 1, &copy_region);
}

void Renderer::create_index_buffer(Mesh* mesh, VkCommandBuffer cmd)
{
	u32 buffer_size = mesh->get_index_buffer_size();
	mesh->index_buffer = context->allocate_buffer(buffer_size,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_INDEX_BUFFER_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
		| VK_BUFFER_USAGE_TRANSFER_DST_BIT
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(context->allocator, mesh->index_buffer.buffer, mesh->index_buffer.allocation);
		}, Garbage_Collector::SHUTDOWN);

	mesh->index_buffer_address = context->get_buffer_device_address(mesh->index_buffer);

	Vk_Allocated_Buffer tmp_staging_buffer = context->allocate_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 0);

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(context->allocator, tmp_staging_buffer.buffer, tmp_staging_buffer.allocation);
		}, Garbage_Collector::SHUTDOWN);

	void* mapped;
	vmaMapMemory(context->allocator, tmp_staging_buffer.allocation, &mapped);
	memcpy(mapped, mesh->indices.data(), buffer_size);
	vmaUnmapMemory(context->allocator, tmp_staging_buffer.allocation);

	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0; // Optional
	copy_region.dstOffset = 0; // Optional
	copy_region.size = buffer_size;

	vkCmdCopyBuffer(cmd, tmp_staging_buffer.buffer, mesh->index_buffer.buffer, 1, &copy_region);
}

void Renderer::create_bottom_level_acceleration_structure(Mesh* mesh)
{

	VkAccelerationStructureBuildGeometryInfoKHR build_info{};
	VkAccelerationStructureGeometryKHR geometry{};
	VkAccelerationStructureBuildRangeInfoKHR range_info{};
	mesh->get_acceleration_structure_build_info(&build_info, &geometry, &range_info);

	VkAccelerationStructureBuildSizesInfoKHR size_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(context->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, &range_info.primitiveCount, &size_info);

	Vk_Allocated_Buffer buffer_blas = context->allocate_buffer((uint32_t)size_info.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(context->allocator, buffer_blas.buffer, buffer_blas.allocation);
		}, Garbage_Collector::SHUTDOWN);

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
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(context->allocator, scratch_buffer.buffer, scratch_buffer.allocation);
		}, Garbage_Collector::SHUTDOWN);

	Vk_Acceleration_Structure as{};
	as.level = Vk_Acceleration_Structure::Level::BOTTOM_LEVEL;
	as.acceleration_structure = acceleration_structure;
	as.scratch_buffer = scratch_buffer;
	as.scratch_buffer_address = context->get_buffer_device_address(scratch_buffer);
	as.acceleration_structure_buffer = buffer_blas;
	as.acceleration_structure_buffer_address = context->get_acceleration_structure_device_address(acceleration_structure);

	mesh->blas = as;
}


void Renderer::build_bottom_level_acceleration_structure(Mesh* mesh, VkCommandBuffer cmd)
{
	VkAccelerationStructureBuildGeometryInfoKHR build_info{};
	VkAccelerationStructureGeometryKHR geometry{};
	VkAccelerationStructureBuildRangeInfoKHR range_info{};
	mesh->get_acceleration_structure_build_info(&build_info, &geometry, &range_info);

	build_info.dstAccelerationStructure = mesh->blas.value().acceleration_structure;
	build_info.scratchData.deviceAddress = mesh->blas.value().scratch_buffer_address;

	VkAccelerationStructureBuildRangeInfoKHR* p_range_info = &range_info;
	vkCmdBuildAccelerationStructuresKHR(
		cmd,
		1,
		&build_info,
		&p_range_info
	);
}

void Renderer::create_top_level_acceleration_structure(ECS* ecs, VkCommandBuffer cmd)
{
	std::vector<VkAccelerationStructureInstanceKHR> instances;

	u32 index = 0;
	VkAccelerationStructureInstanceKHR instance{};
	for (auto [mesh, xform] : ecs->filter<Static_Mesh_Component, Transform_Component>())
	{
		Mesh* m = mesh->manager->get_resource_with_id(mesh->mesh_id);

		VkAccelerationStructureInstanceKHR instance{};
		instance.transform.matrix[0][0] = 
			instance.transform.matrix[1][1] = 
			instance.transform.matrix[2][2] = 1.f;

		instance.instanceCustomIndex = index++;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = m->blas.value().acceleration_structure_buffer_address;

		instances.push_back(instance);
	}

	size_t size = sizeof(instances[0]) * instances.size();
	GPU_Buffer instance_buf = context->create_gpu_buffer(
		(u32)size,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		0
	);

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(context->allocator, instance_buf.gpu_buffer.buffer, instance_buf.gpu_buffer.allocation);
			vmaDestroyBuffer(context->allocator, instance_buf.staging_buffer.buffer, instance_buf.staging_buffer.allocation);
		}, Garbage_Collector::SHUTDOWN);

	void* mapped;
	vmaMapMemory(context->allocator, instance_buf.staging_buffer.allocation, &mapped);
	memcpy(mapped, instances.data(), size);
	vmaUnmapMemory(context->allocator, instance_buf.staging_buffer.allocation);

	instance_buf.upload(cmd);

	vkinit::memory_barrier(cmd,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

	VkAccelerationStructureBuildRangeInfoKHR range_info{};
	range_info.primitiveOffset = 0;
	range_info.primitiveCount = 1;
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

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(context->allocator, buffer_tlas.buffer, buffer_tlas.allocation);
		}, Garbage_Collector::SHUTDOWN);

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

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(context->allocator, scratch.buffer, scratch.allocation);
		}, Garbage_Collector::SHUTDOWN);

	build_info.scratchData.deviceAddress = context->get_buffer_device_address(scratch);

	VkAccelerationStructureBuildRangeInfoKHR* p_range_info = &range_info;
	
	vkCmdBuildAccelerationStructuresKHR(
		cmd,
		1, &build_info,
		&p_range_info
	);



	Vk_Acceleration_Structure scene_tlas{};
	scene_tlas.acceleration_structure = tlas;
	scene_tlas.acceleration_structure_buffer = buffer_tlas;
	scene_tlas.acceleration_structure_buffer_address = context->get_buffer_device_address(buffer_tlas);
	scene_tlas.scratch_buffer = scratch;
	scene_tlas.scratch_buffer_address = context->get_buffer_device_address(scratch);
	scene_tlas.level = Vk_Acceleration_Structure::TOP_LEVEL;
	//scene_tlas.tlas_instances = instance_buf;
	scene_tlas.tlas_instances_address = context->get_buffer_device_address(instance_buf.gpu_buffer);

	assert(!scene.tlas.has_value());
	scene.tlas = scene_tlas;
}

void Mesh::get_acceleration_structure_build_info(
	VkAccelerationStructureBuildGeometryInfoKHR* build_info,
	VkAccelerationStructureGeometryKHR* geometry,
	VkAccelerationStructureBuildRangeInfoKHR* range_info)
{
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vertex_buffer_address;
	triangles.vertexStride = get_vertex_size();
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = index_buffer_address;
	triangles.maxVertex = get_vertex_count() - 1;
	triangles.transformData = { 0 };

	*geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry->geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry->geometry.triangles = triangles;
	geometry->flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	*range_info = {};
	range_info->firstVertex = 0;
	range_info->primitiveCount = get_primitive_count();
	range_info->primitiveOffset = 0;
	range_info->transformOffset = 0;

	*build_info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	build_info->flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	build_info->geometryCount = 1;
	build_info->pGeometries = geometry;
	build_info->mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	build_info->type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	build_info->srcAccelerationStructure = VK_NULL_HANDLE;
}

uint32_t Mesh::get_vertex_count()
{
	return (uint32_t)vertices.size();
}

uint32_t Mesh::get_vertex_size()
{
	return (uint32_t)(sizeof(vertices[0]));
}

uint32_t Mesh::get_primitive_count()
{
	return (uint32_t)(indices.size() / 3);
}
