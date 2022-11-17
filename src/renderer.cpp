#include "renderer.h"
#include "common.h"
#include "vk_helpers.h"

#include <assert.h>
#include <array>
#include <r_mesh.h>
#include <ecs.h>

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	printf("validation layer: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

static void vk_transition_layout(
	VkCommandBuffer cmd,
	VkImage img,
	VkImageLayout src_layout,
	VkImageLayout dst_layout,
	VkAccessFlags src_access_mask,
	VkAccessFlags dst_access_mask,
	VkPipelineStageFlags src_stage_mask,
	VkPipelineStageFlags dst_stage_mask);
static bool check_extensions(const std::vector<const char*>& device_exts, const std::vector<VkExtensionProperties>& props);
static void vk_begin_command_buffer(VkCommandBuffer cmd);

void Renderer::create_vk_instance()
{
	VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 3);
	app_info.pApplicationName = "GigaRay";
	app_info.pEngineName = "GigaEngine";
	app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	create_info.pApplicationInfo = &app_info;

	uint32_t count;
	SDL_Vulkan_GetInstanceExtensions(context.window, &count, nullptr);
	assert(count != 0);
	std::vector<const char*> instance_exts(count);
	SDL_Vulkan_GetInstanceExtensions(context.window, &count, instance_exts.data());
	instance_exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	create_info.enabledExtensionCount = (uint32_t)instance_exts.size();
	create_info.ppEnabledExtensionNames = instance_exts.data();

	const std::vector<const char*> validation_layers = {
		"VK_LAYER_KHRONOS_validation"
	};

	create_info.enabledLayerCount = (uint32_t)validation_layers.size();
	create_info.ppEnabledLayerNames = validation_layers.data();

	VkValidationFeatureEnableEXT enabled[] = { VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT };
	VkValidationFeaturesEXT      features{ VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
	features.disabledValidationFeatureCount = 0;
	features.enabledValidationFeatureCount = 1;
	features.pDisabledValidationFeatures = nullptr;
	features.pEnabledValidationFeatures = enabled;

	features.pNext = create_info.pNext;
	create_info.pNext = &features;

	VkInstance instance;
	VK_CHECK(vkCreateInstance(&create_info, nullptr, &instance));

	volkLoadInstance(instance);

	VkDebugUtilsMessengerCreateInfoEXT ci{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;// | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
	ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;// | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	ci.pfnUserCallback = debug_callback;
	VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &ci, nullptr, &context.debug_messenger));
	context.instance = instance;
}

void Renderer::vk_find_physical_device()
{
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;

	std::vector<const char*> device_exts = {
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
		VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME
	};

	uint32_t count;
	vkEnumeratePhysicalDevices(context.instance, &count, nullptr);
	assert(count != 0);
	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(context.instance, &count, devices.data());

	for (VkPhysicalDevice d : devices)
	{
		uint32_t ext_count;
		vkEnumerateDeviceExtensionProperties(d, nullptr, &ext_count, nullptr);

		std::vector<VkExtensionProperties> ext_props(ext_count);
		vkEnumerateDeviceExtensionProperties(d, nullptr, &ext_count, ext_props.data());

		if (check_extensions(device_exts, ext_props))
		{
			physical_device = d;
			break;
		}
	}
	assert(physical_device != VK_NULL_HANDLE);

	context.physical_device = physical_device;

	VkPhysicalDeviceProperties2 phys_dev_props{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
	};
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_props{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
	};
	phys_dev_props.pNext = &rt_pipeline_props;
	vkGetPhysicalDeviceProperties2(physical_device, &phys_dev_props);

	VkPhysicalDeviceFeatures2 features2{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
	};
	VkPhysicalDeviceVulkan12Features features12{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
	};
	VkPhysicalDeviceVulkan11Features features11
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
	};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR as_feats{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
	};
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_pipeline_feats
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
	};

	features2.pNext = &features12;
	features12.pNext = &features11;
	features11.pNext = &as_feats;
	as_feats.pNext = &rt_pipeline_feats;

	vkGetPhysicalDeviceFeatures2(physical_device, &features2);

	VkDevice dev;

	VkDeviceQueueCreateInfo queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };

	uint32_t queue_fam_count;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_fam_count, nullptr);
	assert(queue_fam_count != 0);
	std::vector<VkQueueFamilyProperties> fam_props(queue_fam_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_fam_count, fam_props.data());
	struct {
		int graphics_idx = -1;
		int compute_idx = -1;
		int transfer_idx = -1;
	} queue_indices;

	int i = 0;
	for (const auto& fam : fam_props)
	{
		if (fam.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			queue_indices.graphics_idx = i;
		if (fam.queueFlags & VK_QUEUE_COMPUTE_BIT)
			queue_indices.compute_idx = i;
		if (fam.queueFlags & VK_QUEUE_TRANSFER_BIT)
			queue_indices.transfer_idx = i;

		if (queue_indices.compute_idx != -1 && queue_indices.graphics_idx != -1 && queue_indices.transfer_idx != -1)
			break;
		++i;
	}

	context.graphics_idx = queue_indices.graphics_idx;
	context.compute_idx = queue_indices.compute_idx;
	context.transfer_idx = queue_indices.transfer_idx;

	float queue_prio = 1.f;
	queue_info.queueFamilyIndex = queue_indices.graphics_idx;
	queue_info.queueCount = 1;
	queue_info.pQueuePriorities = &queue_prio;

	VkDeviceCreateInfo device_create_info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	device_create_info.enabledExtensionCount = (uint32_t)device_exts.size();
	device_create_info.ppEnabledExtensionNames = device_exts.data();
	device_create_info.pEnabledFeatures = nullptr;
	device_create_info.pNext = &features2;
	device_create_info.queueCreateInfoCount = 1;
	device_create_info.pQueueCreateInfos = &queue_info;
	VK_CHECK(vkCreateDevice(physical_device, &device_create_info, nullptr, &dev));

	volkLoadDevice(dev);

	context.device = dev;

	vkGetDeviceQueue(context.device, context.graphics_idx, 0, &context.graphics_queue);
}


void Renderer::vk_init_mem_allocator()
{
	VmaAllocator allocator;
	VmaAllocatorCreateInfo cinfo = {};
	cinfo.instance = context.instance;
	cinfo.device = context.device;
	cinfo.physicalDevice = context.physical_device;
	cinfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	VmaVulkanFunctions vulkan_funcs = {};
	vulkan_funcs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	vulkan_funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	cinfo.pVulkanFunctions = &vulkan_funcs;
	VK_CHECK(vmaCreateAllocator(&cinfo, &allocator));
	context.allocator = allocator;
}

Vk_Allocated_Buffer Renderer::vk_allocate_buffer(uint32_t size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage, VmaAllocationCreateFlags flags)
{
	VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = size;
	buffer_info.usage = usage;

	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = memory_usage;
	alloc_info.flags = flags;

	Vk_Allocated_Buffer buffer;
	VK_CHECK(vmaCreateBuffer(context.allocator, &buffer_info, &alloc_info, &buffer.buffer, &buffer.allocation, nullptr));

	return buffer;
}

// FIXME: Hardcoded viewtype and other things probably. 
Vk_Allocated_Image Renderer::vk_allocate_image(VkExtent3D extent)
{
	VkImageCreateInfo cinfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	cinfo.arrayLayers = 1;
	cinfo.extent = extent;
	cinfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	cinfo.imageType = extent.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
	cinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	cinfo.mipLevels = 1;
	cinfo.samples = VK_SAMPLE_COUNT_1_BIT;
	cinfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo allocinfo{};
	allocinfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	Vk_Allocated_Image img;
	vmaCreateImage(context.allocator, &cinfo, &allocinfo, &img.image, &img.allocation, nullptr);

	VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view_info.format = cinfo.format;
	view_info.image = img.image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkCreateImageView(context.device, &view_info, nullptr, &img.image_view);

	return img;
}

// FIXME: Use staging buffers for this
void Renderer::vk_upload(Vk_Allocated_Buffer* buffer, void* data, uint32_t size)
{
	void* dst;
	vmaMapMemory(context.allocator, buffer->allocation, &dst);
	memcpy(dst, data, (uint32_t)size);
	vmaUnmapMemory(context.allocator, buffer->allocation);
}

VkDeviceAddress Renderer::get_buffer_device_address(const Vk_Allocated_Buffer& buf)
{
	VkBufferDeviceAddressInfo addr_info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	addr_info.buffer = buf.buffer;
	return vkGetBufferDeviceAddress(context.device, &addr_info);
}

void Renderer::vk_create_command_pool()
{
	VkCommandPoolCreateInfo pool_create_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_create_info.queueFamilyIndex = context.graphics_idx;
	pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK(vkCreateCommandPool(context.device, &pool_create_info, nullptr, &context.command_pool));

	VkCommandBufferAllocateInfo cmd_alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmd_alloc_info.commandPool = context.command_pool;
	cmd_alloc_info.commandBufferCount = 1;
	cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VK_CHECK(vkAllocateCommandBuffers(context.device, &cmd_alloc_info, &context.main_command_buffer));

	std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> per_frame_cmd;
	cmd_alloc_info.commandBufferCount = 2;
	VK_CHECK(vkAllocateCommandBuffers(context.device, &cmd_alloc_info, per_frame_cmd.data()));

	for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		context.frame_objects[i].cmd = per_frame_cmd[i];
	}
}

// FIXME: We're gonna need more stuff than this 
void Renderer::vk_create_descriptor_set_layout()
{
	std::array<VkDescriptorSetLayoutBinding, 4> bindings;
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

	VkDescriptorSetLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_info.bindingCount = uint32_t(bindings.size());
	layout_info.pBindings = bindings.data();
	layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	VK_CHECK(vkCreateDescriptorSetLayout(context.device, &layout_info, nullptr, &context.descriptor_set_layout));

	// NOTE: No descriptor pools because we are using push descriptors for now!!
}

VkShaderModule Renderer::vk_create_shader_module_from_file(const char* filepath)
{
	uint8_t* data;
	uint32_t bytes_read = read_entire_file(filepath, &data);
	assert(bytes_read != 0);
	const size_t spirv_size = bytes_read;
	const uint32_t* spirv_data = (uint32_t*)data;

	VkShaderModuleCreateInfo create_info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = spirv_size;
	create_info.pCode = spirv_data;

	VkShaderModule sm;
	VK_CHECK(vkCreateShaderModule(context.device, &create_info, nullptr, &sm));

	return sm;
}

// FIXME: Query for the proper formats.
void Renderer::vk_create_swapchain()
{
	VkSwapchainCreateInfoKHR cinfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	SDL_Vulkan_CreateSurface(context.window, context.instance, &context.surface);

	int w, h;
	SDL_Vulkan_GetDrawableSize(context.window, &w, &h);
	VkExtent2D extent;
	extent.width = w; extent.height = h;
	VkSurfaceFormatKHR surface_format;
	surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	surface_format.format = VK_FORMAT_B8G8R8A8_SRGB;
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

	VkSurfaceCapabilitiesKHR surface_caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physical_device, context.surface, &surface_caps);

	cinfo.surface = context.surface;
	cinfo.minImageCount = surface_caps.minImageCount + 1;
	cinfo.imageFormat = surface_format.format;
	cinfo.imageColorSpace = surface_format.colorSpace;
	cinfo.imageExtent = extent;
	cinfo.imageArrayLayers = 1;
	cinfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	cinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	cinfo.queueFamilyIndexCount = 0;
	cinfo.pQueueFamilyIndices = nullptr;
	cinfo.preTransform = surface_caps.currentTransform;
	cinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	cinfo.presentMode = present_mode;
	cinfo.clipped = VK_TRUE;
	cinfo.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK(vkCreateSwapchainKHR(context.device, &cinfo, nullptr, &context.swapchain));

	uint32_t image_count = 0;
	vkGetSwapchainImagesKHR(context.device, context.swapchain, &image_count, nullptr);
	assert(image_count != 0);
	context.swapchain_images.resize(image_count);
	vkGetSwapchainImagesKHR(context.device, context.swapchain, &image_count, context.swapchain_images.data());

	// Create image views
	context.swapchain_image_views.resize(image_count);
	for (size_t i = 0; i < context.swapchain_images.size(); ++i)
	{
		VkImageViewCreateInfo cinfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		cinfo.image = context.swapchain_images[i];
		cinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		cinfo.format = surface_format.format;
		cinfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		cinfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		cinfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		cinfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		cinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		cinfo.subresourceRange.baseArrayLayer = 0;
		cinfo.subresourceRange.layerCount = 1;
		cinfo.subresourceRange.baseMipLevel = 0;
		cinfo.subresourceRange.levelCount = 1;

		VK_CHECK(vkCreateImageView(context.device, &cinfo, nullptr, &context.swapchain_image_views[i]));
	}

	// Transition images to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR

	vk_begin_command_buffer(context.main_command_buffer);
	for (int i = 0; i < context.swapchain_images.size(); ++i)
	{
		vk_transition_layout(context.main_command_buffer,
			context.swapchain_images[i],
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			0, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	}
	vkEndCommandBuffer(context.main_command_buffer);
	vk_command_buffer_single_submit(context.main_command_buffer);
	vkQueueWaitIdle(context.graphics_queue);
}

VkFence Renderer::vk_create_fence()
{
	VkFenceCreateInfo cinfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	cinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence fence;
	VK_CHECK(vkCreateFence(context.device, &cinfo, nullptr, &fence));

	return fence;
}

VkSemaphore Renderer::vk_create_semaphore()
{
	VkSemaphoreCreateInfo cinfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkSemaphore sem;
	VK_CHECK(vkCreateSemaphore(context.device, &cinfo, nullptr, &sem));
	return sem;
}

// FIXME: Hardcoded shaders and hitgroups
Vk_Pipeline Renderer::vk_create_rt_pipeline()
{
	VkShaderModule rgen_shader = vk_create_shader_module_from_file("shaders/spirv/test.rgen.spv");
	VkShaderModule rmiss_shader = vk_create_shader_module_from_file("shaders/spirv/test.rmiss.spv");
	VkShaderModule rchit_shader = vk_create_shader_module_from_file("shaders/spirv/test.rchit.spv");

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
	cinfo.pSetLayouts = &context.descriptor_set_layout;
	cinfo.setLayoutCount = 1;
	VkPipelineLayout layout;
	vkCreatePipelineLayout(context.device, &cinfo, nullptr, &layout);

	VkRayTracingPipelineCreateInfoKHR rtpci{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	rtpci.stageCount = (uint32_t)pssci.size();
	rtpci.pStages = pssci.data();
	rtpci.groupCount = (uint32_t)rtsgci.size();
	rtpci.pGroups = rtsgci.data();
	rtpci.maxPipelineRayRecursionDepth = 4;
	rtpci.layout = layout;

	VkPipeline rt_pipeline;
	VK_CHECK(vkCreateRayTracingPipelinesKHR(
		context.device,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		1, &rtpci,
		nullptr,
		&rt_pipeline));

	Vk_Pipeline pl;
	pl.layout = layout;
	pl.pipeline = rt_pipeline;

	uint32_t group_count = (uint32_t)rtsgci.size();
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	rt_pipeline_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	VkPhysicalDeviceProperties2 device_props{};
	device_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	device_props.pNext = &rt_pipeline_props;
	vkGetPhysicalDeviceProperties2(context.physical_device, &device_props);
	uint32_t group_handle_size = rt_pipeline_props.shaderGroupHandleSize;
	uint32_t group_size_aligned = (group_handle_size + rt_pipeline_props.shaderGroupBaseAlignment - 1) & ~(rt_pipeline_props.shaderGroupBaseAlignment - 1);

	uint32_t sbt_size = group_count * group_size_aligned;

	std::vector<uint8_t> shader_handle_storage(sbt_size);
	VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(
		context.device,
		rt_pipeline,
		0,
		group_count,
		sbt_size,
		shader_handle_storage.data()
	));

	Vk_Allocated_Buffer rt_sbt_buffer = vk_allocate_buffer(sbt_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
	);

	void* mapped;
	vmaMapMemory(context.allocator, rt_sbt_buffer.allocation, &mapped);
	uint8_t* pdata = (uint8_t*)mapped;
	for (uint32_t g = 0; g < group_count; ++g)
	{
		memcpy(pdata, shader_handle_storage.data() + g * group_handle_size,
			group_handle_size);
		pdata += group_size_aligned;
	}
	vmaUnmapMemory(context.allocator, rt_sbt_buffer.allocation);

	context.shader_binding_table = rt_sbt_buffer;
	context.device_sbt_alignment = group_size_aligned;
	return pl;
}

// Inserts a barrier into the command buffer to perform a specific layout transition
static void vk_transition_layout(
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

void Renderer::vk_create_sync_objects()
{
	for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		context.frame_objects[i].fence = vk_create_fence();
		context.frame_objects[i].image_available_sem = vk_create_semaphore();
		context.frame_objects[i].render_finished_sem = vk_create_semaphore();
	}
}

void Renderer::initialize()
{
	init_sdl();

	SDL_Vulkan_GetDrawableSize(context.window, &context.width, &context.height);

	VK_CHECK(volkInitialize());

	create_vk_instance();
	vk_find_physical_device();
	vk_create_command_pool();
	vk_init_mem_allocator();
	vk_create_swapchain();
	vk_create_descriptor_set_layout();
	vk_create_sync_objects();

	rt_pipeline = vk_create_rt_pipeline();
	vk_create_render_targets();
	vk_create_staging_buffers();

	initialized = true;
}

const StagingBuffer& Renderer::get_staging_buffer()
{
	uint32_t frame_index = get_frame_index();

	return context.frame_objects[frame_index].staging_buffer;
}

VkCommandBuffer Renderer::get_current_frame_command_buffer()
{
	uint32_t frame_index = get_frame_index();
	return context.frame_objects[frame_index].cmd;
}

void Renderer::init_sdl()
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	context.window = SDL_CreateWindow("Vulkan", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
}

void Renderer::vk_command_buffer_single_submit(VkCommandBuffer cmd)
{
	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd;

	vkQueueSubmit(context.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
}

VkDeviceAddress Renderer::vk_get_acceleration_structure_device_address(VkAccelerationStructureKHR as)
{
	VkAccelerationStructureDeviceAddressInfoKHR address_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
	address_info.accelerationStructure = as;
	VkDeviceAddress address = vkGetAccelerationStructureDeviceAddressKHR(context.device, &address_info);
	return address;
}

Vk_Acceleration_Structure Renderer::vk_create_acceleration_structure(Mesh* mesh)
{
	Vk_Acceleration_Structure as{};
	as.level = Vk_Acceleration_Structure::Level::BOTTOM_LEVEL;

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = mesh->vertex_buffer_address;
	triangles.vertexStride = mesh->get_vertex_size();
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = mesh->index_buffer_address;
	triangles.maxVertex = mesh->get_vertex_count() - 1;
	triangles.transformData = { 0 };

	VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.geometry.triangles = triangles;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	VkAccelerationStructureBuildRangeInfoKHR range_info;
	range_info.firstVertex = 0;
	range_info.primitiveCount = mesh->get_primitive_count();
	range_info.primitiveOffset = 0;
	range_info.transformOffset = 0;

	VkAccelerationStructureBuildGeometryInfoKHR build_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	build_info.geometryCount = 1;
	build_info.pGeometries = &geometry;
	build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	build_info.srcAccelerationStructure = VK_NULL_HANDLE;

	VkAccelerationStructureBuildSizesInfoKHR size_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(context.device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, &range_info.primitiveCount, &size_info);

	Vk_Allocated_Buffer buffer_blas = vk_allocate_buffer((uint32_t)size_info.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

	as.acceleration_structure_buffer = buffer_blas;

	VkAccelerationStructureCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	create_info.type = build_info.type;
	create_info.size = size_info.accelerationStructureSize;
	create_info.buffer = buffer_blas.buffer;
	create_info.offset = 0;
	VK_CHECK(vkCreateAccelerationStructureKHR(context.device, &create_info, nullptr, &as.acceleration_structure));
	build_info.dstAccelerationStructure = as.acceleration_structure;

	Vk_Allocated_Buffer scratch_buffer = as.scratch_buffer = vk_allocate_buffer((uint32_t)size_info.buildScratchSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
	build_info.scratchData.deviceAddress = as.scratch_buffer_address = get_buffer_device_address(scratch_buffer);


	VkAccelerationStructureBuildRangeInfoKHR* p_range_info = &range_info;
	vk_begin_command_buffer(context.main_command_buffer);
	vkCmdBuildAccelerationStructuresKHR(context.main_command_buffer, 1, &build_info, &p_range_info);

	vkEndCommandBuffer(context.main_command_buffer);
	vk_command_buffer_single_submit(context.main_command_buffer);

	// FIXME: Do something about this...
	vkQueueWaitIdle(context.graphics_queue);

	as.acceleration_structure_buffer_address = vk_get_acceleration_structure_device_address(as.acceleration_structure);

	return as;
}

void Renderer::begin_frame()
{
	uint32_t frame_index = get_frame_index();
	vkWaitForFences(context.device, 1, &context.frame_objects[frame_index].fence, VK_TRUE, UINT64_MAX);
	vkResetFences(context.device, 1, &context.frame_objects[frame_index].fence);
	vkAcquireNextImageKHR(context.device, context.swapchain,
		UINT64_MAX, context.frame_objects[frame_index].image_available_sem,
		VK_NULL_HANDLE, &swapchain_image_index);

	VkCommandBuffer cmd = get_current_frame_command_buffer();
	VkImage next_image = context.swapchain_images[swapchain_image_index];

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

	VkImage next_image = context.swapchain_images[swapchain_image_index];
	vkCmdBlitImage(cmd, 
		framebuffer.render_targets[0].image.image, VK_IMAGE_LAYOUT_GENERAL, next_image, VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_LINEAR);
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
	submit.pWaitSemaphores = &context.frame_objects[frame_index].image_available_sem;
	submit.pSignalSemaphores = &context.frame_objects[frame_index].render_finished_sem;

	vkEndCommandBuffer(cmd);

	vkQueueSubmit(context.graphics_queue, 1, &submit, context.frame_objects[frame_index].fence);

	VkPresentInfoKHR present_info{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &context.swapchain;
	present_info.pImageIndices = &swapchain_image_index;

	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &context.frame_objects[frame_index].render_finished_sem;
	vkQueuePresentKHR(context.graphics_queue, &present_info);

	frame_counter++;
}

void Renderer::draw(ECS* ecs)
{
	VkCommandBuffer cmd = get_current_frame_command_buffer();

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline.pipeline);

	// Update descriptor set

	VkWriteDescriptorSet writes[4] = { };
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

	vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline.layout, 0, (uint32_t)std::size(writes), writes);

	// Trace rays

	VkDeviceAddress base = get_buffer_device_address(context.shader_binding_table);
	VkStridedDeviceAddressRegionKHR raygen_region{};
	raygen_region.deviceAddress = base + 0 * context.device_sbt_alignment;
	raygen_region.stride = 32;
	raygen_region.size = 32;

	VkStridedDeviceAddressRegionKHR miss_region{};
	miss_region.deviceAddress = base + 1 * context.device_sbt_alignment;
	miss_region.stride = 32;
	miss_region.size = 32;

	VkStridedDeviceAddressRegionKHR hit_region{};
	hit_region.deviceAddress = base + 2 * context.device_sbt_alignment;
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
}

Vk_Acceleration_Structure Renderer::vk_create_top_level_acceleration_structure(Mesh* mesh)
{
	assert(mesh->blas.has_value());
	VkAccelerationStructureInstanceKHR instance = vkinit::acceleration_structure_instance(mesh->blas.value().acceleration_structure_buffer_address);
	
	Vk_Allocated_Buffer buffer_instances = vk_allocate_buffer((uint32_t)sizeof(instance),
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
	vk_upload_cpu_to_gpu(buffer_instances.buffer, &instance, (uint32_t)sizeof(instance));

	VkAccelerationStructureBuildRangeInfoKHR range_info = {};
	range_info.primitiveOffset = 0;
	range_info.primitiveCount = 1;
	range_info.firstVertex = 0;
	range_info.transformOffset = 0;

	VkAccelerationStructureGeometryInstancesDataKHR instance_vk{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	instance_vk.arrayOfPointers = VK_FALSE;
	instance_vk.data.deviceAddress = get_buffer_device_address(buffer_instances);

	VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances = instance_vk;

	VkAccelerationStructureBuildGeometryInfoKHR build_geom_info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	build_geom_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	build_geom_info.geometryCount = 1;
	build_geom_info.pGeometries = &geometry;
	build_geom_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	build_geom_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	build_geom_info.srcAccelerationStructure = VK_NULL_HANDLE;

	VkAccelerationStructureBuildSizesInfoKHR size_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(
		context.device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&build_geom_info,
		&range_info.primitiveCount,
		&size_info
	);

	Vk_Allocated_Buffer buffer_tlas = vk_allocate_buffer((uint32_t)size_info.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

	VkAccelerationStructureKHR tlas;
	VkAccelerationStructureCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	create_info.type = build_geom_info.type;
	create_info.size = size_info.accelerationStructureSize;
	create_info.buffer = buffer_tlas.buffer;
	create_info.offset = 0;
	VK_CHECK(vkCreateAccelerationStructureKHR(context.device, &create_info, nullptr, &tlas));

	build_geom_info.dstAccelerationStructure = tlas;

	Vk_Allocated_Buffer scratch_buffer = vk_allocate_buffer((uint32_t)size_info.buildScratchSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
	build_geom_info.scratchData.deviceAddress = get_buffer_device_address(scratch_buffer);

	vk_begin_command_buffer(context.main_command_buffer);

	VkAccelerationStructureBuildRangeInfoKHR* p_range_info = &range_info;

	vkCmdBuildAccelerationStructuresKHR(context.main_command_buffer, 1, &build_geom_info, &p_range_info);

	vkEndCommandBuffer(context.main_command_buffer);

	vk_command_buffer_single_submit(context.main_command_buffer);

	vkQueueWaitIdle(context.graphics_queue);

	Vk_Acceleration_Structure as{};

	as.level = Vk_Acceleration_Structure::Level::TOP_LEVEL;
	as.acceleration_structure = tlas;
	as.acceleration_structure_buffer = buffer_tlas;
	as.acceleration_structure_buffer_address = get_buffer_device_address(buffer_tlas);
	as.scratch_buffer = scratch_buffer;
	as.scratch_buffer_address = get_buffer_device_address(scratch_buffer);
	as.tlas_instances = buffer_instances;
	as.tlas_instances_address = get_buffer_device_address(buffer_instances);

	return as;
}

void Renderer::vk_upload_cpu_to_gpu(VkBuffer dst, void* data, uint32_t size)
{
	const StagingBuffer& buf = get_staging_buffer();

	void* mapped;
	vmaMapMemory(context.allocator, buf.buffer.allocation, &mapped);
	memcpy(mapped, data, size);
	vmaUnmapMemory(context.allocator, buf.buffer.allocation);

	vk_begin_command_buffer(buf.cmd);

	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0; // Optional
	copy_region.dstOffset = 0; // Optional
	copy_region.size = size;

	vkCmdCopyBuffer(buf.cmd, buf.buffer.buffer, dst, 1, &copy_region);

	vkEndCommandBuffer(buf.cmd);

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &buf.cmd;

	vkQueueSubmit(context.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

	// FIXME: Improve synchronization
	vkQueueWaitIdle(context.graphics_queue);
}

void Renderer::vk_create_staging_buffers()
{
	for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		constexpr uint32_t staging_buffer_size = 256 * 1024 * 1024; // 256 MB
		Vk_Allocated_Buffer staging_buffer = vk_allocate_buffer(
			staging_buffer_size, 
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			VMA_MEMORY_USAGE_AUTO, 
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
		);
		StagingBuffer sb{};
		sb.buffer = staging_buffer;

		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandPool = context.command_pool;
		alloc_info.commandBufferCount = 1;
		vkAllocateCommandBuffers(context.device, &alloc_info, &sb.cmd);

		context.frame_objects[i].staging_buffer = sb;
	}
}

static void vk_begin_command_buffer(VkCommandBuffer cmd)
{
	VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(cmd, &begin_info);
}

void Renderer::vk_create_render_targets()
{
	framebuffer.render_targets.resize(1);

	Vk_RenderTarget color_attachment;
	color_attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
	color_attachment.image = vk_allocate_image({ (uint32_t)context.width, (uint32_t)context.height, 1 });

	VkCommandBuffer cmd = context.main_command_buffer;
	vk_begin_command_buffer(cmd);
	vk_transition_layout(cmd, color_attachment.image.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

	vkEndCommandBuffer(cmd);
	vk_command_buffer_single_submit(cmd);
	vkQueueWaitIdle(context.graphics_queue);
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
	return (uint32_t)(sizeof(glm::vec3) * vertices.size());
}

uint32_t Mesh::get_index_buffer_size()
{
	return (uint32_t)(sizeof(uint32_t) * indices.size());
}

void Mesh::create_vertex_buffer(Renderer* renderer)
{
	vertex_buffer = renderer->vk_allocate_buffer(get_vertex_buffer_size(),
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

	renderer->vk_upload_cpu_to_gpu(vertex_buffer.buffer, vertices.data(), get_vertex_buffer_size());
	vertex_buffer_address = renderer->get_buffer_device_address(vertex_buffer);
}

void Mesh::create_index_buffer(Renderer* renderer)
{
	index_buffer = renderer->vk_allocate_buffer(get_index_buffer_size(),
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_INDEX_BUFFER_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
		| VK_BUFFER_USAGE_TRANSFER_DST_BIT
		| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

	renderer->vk_upload_cpu_to_gpu(index_buffer.buffer, indices.data(), get_index_buffer_size());
	index_buffer_address = renderer->get_buffer_device_address(index_buffer);
}

void Mesh::build_bottom_level_acceleration_structure()
{
	blas = renderer->vk_create_acceleration_structure(this);
}

uint32_t Mesh::get_vertex_count()
{
	return (uint32_t)vertices.size();
}

uint32_t Mesh::get_vertex_size()
{
	return (uint32_t)(sizeof(glm::vec3));
}

uint32_t Mesh::get_primitive_count()
{
	return (uint32_t)(indices.size() / 3);
}
