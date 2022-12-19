#include "r_vulkan.h"
#include "common.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "vk_helpers.h"
#include "shaders.h"

#define VSYNC 0

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	printf("validation layer: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

static bool check_extensions(const std::vector<const char*>& device_exts, const std::vector<VkExtensionProperties>& props);

Vk_Context::Vk_Context(Platform* platform)
{
	VK_CHECK(volkInitialize());

	create_instance(&platform->window);
	find_physical_device();
	create_command_pool();
	init_mem_allocator();
	create_swapchain(platform);
	create_sync_objects();
}

void Vk_Context::create_instance(Platform_Window* window)
{
	VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 3);
	app_info.pApplicationName = APP_NAME;
	app_info.pEngineName = ENGINE_NAME;
	app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	create_info.pApplicationInfo = &app_info;

	uint32_t count;
	window->get_vulkan_window_surface_extensions(&count, nullptr);
	assert(count != 0);
	std::vector<const char*> instance_exts(count);
	window->get_vulkan_window_surface_extensions(&count, instance_exts.data());

	if constexpr(USE_VALIDATION_LAYERS)
		instance_exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	create_info.enabledExtensionCount = (uint32_t)instance_exts.size();
	create_info.ppEnabledExtensionNames = instance_exts.data();

	std::vector<const char*> layers{};
	VkValidationFeatureEnableEXT enabled[] = { VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT };
	VkValidationFeaturesEXT      features{ VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
	if constexpr (USE_VALIDATION_LAYERS)
	{
		layers.push_back("VK_LAYER_KHRONOS_validation");

		create_info.enabledLayerCount = (uint32_t)layers.size();
		create_info.ppEnabledLayerNames = layers.data();
		features.disabledValidationFeatureCount = 0;
		features.enabledValidationFeatureCount = 1;
		features.pDisabledValidationFeatures = nullptr;
		features.pEnabledValidationFeatures = enabled;
		features.pNext = create_info.pNext;
		create_info.pNext = &features;
	}

	VkInstance instance;
	VK_CHECK(vkCreateInstance(&create_info, nullptr, &instance));
	g_garbage_collector->push([instance]() {
		vkDestroyInstance(instance, nullptr);
		}, Garbage_Collector::SHUTDOWN);

	volkLoadInstance(instance);

	if constexpr (USE_VALIDATION_LAYERS)
	{
		VkDebugUtilsMessengerCreateInfoEXT ci{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
		ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		ci.pfnUserCallback = debug_callback;
		VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &ci, nullptr, &debug_messenger));
		g_garbage_collector->push([=]()
			{
				vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
			}, Garbage_Collector::SHUTDOWN);
	}

	this->instance = instance;
}

void Vk_Context::find_physical_device()
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
	vkEnumeratePhysicalDevices(instance, &count, nullptr);
	assert(count != 0);
	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(instance, &count, devices.data());

	// Find first device with required extensions
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

	this->physical_device = physical_device;

	VkPhysicalDeviceProperties2 phys_dev_props{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
	};
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_props{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
	};
	phys_dev_props.pNext = &rt_pipeline_props;
	vkGetPhysicalDeviceProperties2(physical_device, &phys_dev_props);
	physical_device_properties = phys_dev_props;

	VkPhysicalDeviceFeatures2 features2{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
	};
	VkPhysicalDeviceVulkan13Features features13
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
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
	VkPhysicalDeviceMaintenance4Features maintenance_feats{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES
	};


	features2.pNext = &features13;
	features13.pNext = &features12;
	features12.pNext = &features11;
	features11.pNext = &as_feats;
	as_feats.pNext = &rt_pipeline_feats;

	vkGetPhysicalDeviceFeatures2(physical_device, &features2);

	device_sbt_alignment = rt_pipeline_props.shaderGroupBaseAlignment;
	device_shader_group_handle_size = rt_pipeline_props.shaderGroupHandleSize;

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

	graphics_idx = queue_indices.graphics_idx;
	compute_idx = queue_indices.compute_idx;
	transfer_idx = queue_indices.transfer_idx;

	constexpr u32 queue_count = 2; // 1 primary, 1 async;
	std::array<float, 2> queue_prio = { 1.f, 1.f };
	queue_info.queueFamilyIndex = queue_indices.graphics_idx;
	queue_info.queueCount = (u32)queue_prio.size();
	queue_info.pQueuePriorities = queue_prio.data();

	VkDeviceCreateInfo device_create_info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	device_create_info.enabledExtensionCount = (uint32_t)device_exts.size();
	device_create_info.ppEnabledExtensionNames = device_exts.data();
	device_create_info.pEnabledFeatures = nullptr;
	device_create_info.pNext = &features2;
	device_create_info.queueCreateInfoCount = 1;
	device_create_info.pQueueCreateInfos = &queue_info;
	VK_CHECK(vkCreateDevice(physical_device, &device_create_info, nullptr, &dev));
	g_garbage_collector->push([dev]()
		{
			vkDestroyDevice(dev, nullptr);
		}
	, Garbage_Collector::SHUTDOWN);

	volkLoadDevice(dev);

	device = dev;

	vkGetDeviceQueue(device, graphics_idx, 0, &graphics_queue);
	vkGetDeviceQueue(device, graphics_idx, 1, &async_upload.upload_queue);
}

void Vk_Context::init_mem_allocator()
{
	VmaAllocator allocator;
	VmaAllocatorCreateInfo cinfo = {};
	cinfo.instance = instance;
	cinfo.device = device;
	cinfo.physicalDevice = physical_device;
	cinfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	VmaVulkanFunctions vulkan_funcs = {};
	vulkan_funcs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	vulkan_funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	cinfo.pVulkanFunctions = &vulkan_funcs;
	VK_CHECK(vmaCreateAllocator(&cinfo, &allocator));
	this->allocator = allocator;
	g_garbage_collector->push([=]()
		{
			vmaDestroyAllocator(allocator);
		}, Garbage_Collector::SHUTDOWN);
}

void Vk_Context::create_command_pool()
{
	VkCommandPoolCreateInfo pool_create_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_create_info.queueFamilyIndex = graphics_idx;
	pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK(vkCreateCommandPool(device, &pool_create_info, nullptr, &command_pool));
	VK_CHECK(vkCreateCommandPool(device, &pool_create_info, nullptr, &async_command_pool));
	g_garbage_collector->push([=]()
		{
			vkDestroyCommandPool(device, command_pool, nullptr);
			vkDestroyCommandPool(device, async_command_pool, nullptr);

		},
		Garbage_Collector::SHUTDOWN);

	VkCommandBufferAllocateInfo cmd_alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmd_alloc_info.commandPool = command_pool;
	cmd_alloc_info.commandBufferCount = FRAMES_IN_FLIGHT;
	cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> per_frame_cmd;
	VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc_info, per_frame_cmd.data()));

	for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		frame_objects[i].cmd = per_frame_cmd[i];
	}
}

static VkSurfaceFormatKHR get_surface_format(const std::vector<VkSurfaceFormatKHR>& formats)
{
	for (const auto& f : formats)
	{
		if (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
			f.format == VK_FORMAT_B8G8R8A8_UNORM)
		{
			return f;
		}
	}

	return formats[0];
}

static VkPresentModeKHR get_present_mode(const std::vector<VkPresentModeKHR>& present_modes)
{
#if VSYNC == 1
	return VK_PRESENT_MODE_FIFO_KHR;
#else
	for (const auto& m : present_modes)
	{
		if (m == VK_PRESENT_MODE_MAILBOX_KHR)
			return m;
	}

	return VK_PRESENT_MODE_FIFO_KHR;
#endif
}

void Vk_Context::create_swapchain(Platform* platform)
{
	VkSwapchainCreateInfoKHR cinfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };

	platform->create_vulkan_surface(instance, &surface);
	g_garbage_collector->push([=]() {
		vkDestroySurfaceKHR(instance, surface, nullptr);
		}, Garbage_Collector::SHUTDOWN);
	int w, h;
	platform->get_window_size(&w, &h);
	VkExtent2D extent;
	extent.width = w; extent.height = h;

	VkSurfaceCapabilitiesKHR surface_caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps);

	u32 format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
	assert(format_count != 0);
	std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, surface_formats.data());

	u32 present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
	assert(present_mode_count != 0);
	std::vector<VkPresentModeKHR> present_modes(present_mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes.data());

	VkSurfaceFormatKHR surface_format = get_surface_format(surface_formats);
	VkPresentModeKHR present_mode = get_present_mode(present_modes);

	cinfo.surface = surface;
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

	VK_CHECK(vkCreateSwapchainKHR(device, &cinfo, nullptr, &swapchain));
	g_garbage_collector->push([=]()
		{
			vkDestroySwapchainKHR(device, swapchain, nullptr);
		}, Garbage_Collector::SHUTDOWN);

	uint32_t image_count = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
	assert(image_count != 0);
	swapchain_images.resize(image_count);
	vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data());

	// Create image views
	swapchain_image_views.resize(image_count);
	for (size_t i = 0; i < swapchain_images.size(); ++i)
	{
		VkImageViewCreateInfo cinfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		cinfo.image = swapchain_images[i];
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

		VK_CHECK(vkCreateImageView(device, &cinfo, nullptr, &swapchain_image_views[i]));
		
		g_garbage_collector->push([=]()
			{
				vkDestroyImageView(device, swapchain_image_views[i], nullptr);
			}, Garbage_Collector::SHUTDOWN);
	}

	// Note: Swapchain images are in VK_IMAGE_LAYOUT_UNDEFINED initially
}

void Vk_Context::create_sync_objects()
{
	for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		frame_objects[i].fence = create_fence();
		frame_objects[i].image_available_sem = create_semaphore();
		frame_objects[i].render_finished_sem = create_semaphore();

		g_garbage_collector->push([=]()
			{
				vkDestroyFence(device, frame_objects[i].fence, nullptr);
				vkDestroySemaphore(device, frame_objects[i].image_available_sem, nullptr);
				vkDestroySemaphore(device, frame_objects[i].render_finished_sem, nullptr);
			}, Garbage_Collector::SHUTDOWN);
	}


	async_upload.timeline_sem = create_semaphore(true);
	async_upload.timeline_semaphore_value = 0;
	g_garbage_collector->push([=]()
		{
			vkDestroySemaphore(device, async_upload.timeline_sem, nullptr);
		}, Garbage_Collector::SHUTDOWN);
}

Vk_Allocated_Buffer Vk_Context::allocate_buffer(uint32_t size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage, VmaAllocationCreateFlags flags, u64 alignment)
{
	VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = size;
	buffer_info.usage = usage;

	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = memory_usage;
	alloc_info.flags = flags;

	Vk_Allocated_Buffer buffer;
	VK_CHECK(vmaCreateBufferWithAlignment(allocator, &buffer_info, &alloc_info, alignment, &buffer.buffer, &buffer.allocation, nullptr));

	g_garbage_collector->push([=]()
		{
			vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
		}, Garbage_Collector::SHUTDOWN);

	return buffer;
}

Vk_Allocated_Image Vk_Context::allocate_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, VkImageTiling tiling, int mip_levels)
{
	VkImageCreateInfo cinfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	cinfo.arrayLayers = 1;
	cinfo.extent = extent;
	cinfo.format = format;
	cinfo.imageType = extent.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
	cinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	cinfo.mipLevels = mip_levels;
	cinfo.samples = VK_SAMPLE_COUNT_1_BIT;
	cinfo.usage = usage; //VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	cinfo.tiling = tiling;

	VmaAllocationCreateInfo allocinfo{};
	allocinfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	Vk_Allocated_Image img;
	vmaCreateImage(allocator, &cinfo, &allocinfo, &img.image, &img.allocation, nullptr);

	VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view_info.format = cinfo.format;
	view_info.image = img.image;
	view_info.viewType = extent.depth == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = mip_levels;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkCreateImageView(device, &view_info, nullptr, &img.image_view);

	g_garbage_collector->push([=]()
		{
			vkDestroyImageView(device, img.image_view, nullptr);
			vmaDestroyImage(allocator, img.image, img.allocation);
		}, Garbage_Collector::SHUTDOWN);

	return img;
}

Vk_Allocated_Buffer Vk_Context::create_buffer(VkCommandBuffer cmd, size_t size, void* data, VkBufferUsageFlags usage)
{
	Vk_Allocated_Buffer buf = allocate_buffer((u32)size, usage,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

	Vk_Allocated_Buffer tmp_staging_buffer = allocate_buffer(
		(u32)size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 0);

	void* mapped;
	vmaMapMemory(allocator, tmp_staging_buffer.allocation, &mapped);
	memcpy(mapped, data, size);
	vmaUnmapMemory(allocator, tmp_staging_buffer.allocation);

	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0; // Optional
	copy_region.dstOffset = 0; // Optional
	copy_region.size = size;

	vkCmdCopyBuffer(cmd, tmp_staging_buffer.buffer, buf.buffer, 1, &copy_region);

	return buf;
}

VkDeviceAddress Vk_Context::get_buffer_device_address(const Vk_Allocated_Buffer& buf)
{
	VkBufferDeviceAddressInfo addr_info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	addr_info.buffer = buf.buffer;
	return vkGetBufferDeviceAddress(device, &addr_info);
}

VkShaderModule Vk_Context::create_shader_module_from_file(const char* filepath)
{
	// FIXME: This should go through some kind of filesystem
	uint8_t* data;
	uint32_t bytes_read = read_entire_file(filepath, &data);
	assert(bytes_read != 0);
	const size_t spirv_size = bytes_read;
	const uint32_t* spirv_data = (uint32_t*)data;

	VkShaderModuleCreateInfo create_info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = spirv_size;
	create_info.pCode = spirv_data;

	VkShaderModule sm;
	VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &sm));

	free(data);

	return sm;
}

VkShaderModule Vk_Context::create_shader_module_from_bytes(u32* bytes, size_t size)
{
	VkShaderModuleCreateInfo create_info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = size;
	create_info.pCode = bytes;

	VkShaderModule sm;
	VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &sm));
	return sm;
}

VkDescriptorUpdateTemplate Vk_Context::create_descriptor_update_template(u32 shader_count, Shader* shaders, VkPipelineBindPoint bind_point, VkPipelineLayout pipeline_layout)
{
	VkDescriptorUpdateTemplate update_template;

	u32 resource_mask = 0;
	for (u32 i = 0; i < shader_count; ++i)
		resource_mask |= shaders[i].resource_mask;
	// Find highest set bit
	u32 n = resource_mask;
	u32 msb_pos = 0;
	while (n >>= 1)
		msb_pos++;

	int count = msb_pos + 1;

	std::vector<VkDescriptorUpdateTemplateEntry> entries(count, VkDescriptorUpdateTemplateEntry());
	
	for (int i = 0; i < count; ++i)
	{
		for (u32 j = 0; j < shader_count; ++j)
		{
			Shader* shader = &shaders[j];
			if (shader->resource_mask & (1 << i))
			{
				entries[i].descriptorCount = 1;
				entries[i].descriptorType = shader->descriptor_types[i];
				entries[i].dstArrayElement = 0;
				entries[i].dstBinding = i;
				entries[i].offset = sizeof(Descriptor_Info) * i;
				entries[i].stride = sizeof(Descriptor_Info);
			}
		}
	}

	VkDescriptorUpdateTemplateCreateInfo cinfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };
	cinfo.descriptorUpdateEntryCount = count;
	cinfo.pDescriptorUpdateEntries = entries.data();
	cinfo.pipelineBindPoint = bind_point;
	cinfo.pipelineLayout = pipeline_layout;
	cinfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
	cinfo.set = 0;
	vkCreateDescriptorUpdateTemplate(device, &cinfo, nullptr, &update_template);

	return update_template;
}

VkQueryPool Vk_Context::create_query_pool()
{
	constexpr u32 query_count = 128;
	VkQueryPool query_pool;
	VkQueryPoolCreateInfo cinfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	cinfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	cinfo.queryCount = query_count;
	vkCreateQueryPool(device, &cinfo, nullptr, &query_pool);

	return query_pool;
}

VkFence Vk_Context::create_fence()
{
	VkFenceCreateInfo cinfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	cinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence fence;
	VK_CHECK(vkCreateFence(device, &cinfo, nullptr, &fence));

	return fence;
}

VkDeviceAddress Vk_Context::get_acceleration_structure_device_address(VkAccelerationStructureKHR as)
{
	VkAccelerationStructureDeviceAddressInfoKHR address_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
	address_info.accelerationStructure = as;
	VkDeviceAddress address = vkGetAccelerationStructureDeviceAddressKHR(device, &address_info);
	return address;
}

VkSemaphore Vk_Context::create_semaphore(bool timeline_sem)
{
	VkSemaphoreCreateInfo cinfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkSemaphoreTypeCreateInfo timelineCreateInfo{};
	if (timeline_sem)
	{
		timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		timelineCreateInfo.pNext = NULL;
		timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		timelineCreateInfo.initialValue = 0;
		cinfo.pNext = &timelineCreateInfo;
	}

	VkSemaphore sem;
	VK_CHECK(vkCreateSemaphore(device, &cinfo, nullptr, &sem));
	return sem;
}

GPU_Buffer Vk_Context::create_gpu_buffer(u32 size, VkBufferUsageFlags usage_flags, u32 alignment)
{
	GPU_Buffer buffer{};

	buffer.size = (size_t)size;
	buffer.gpu_buffer = allocate_buffer(size, usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO, 0, alignment);
	for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
		buffer.staging_buffer[i] = allocate_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, alignment);

	return buffer;
}

void Vk_Context::free_image(Vk_Allocated_Image img)
{
	vmaFreeMemory(allocator, img.allocation);
}

void Vk_Context::free_buffer(Vk_Allocated_Buffer buffer)
{
	vmaFreeMemory(allocator, buffer.allocation);
}

VkDescriptorSetLayout Vk_Context::create_descriptor_set_layout(u32 num_shaders, Shader* shaders)
{
	VkDescriptorSetLayoutCreateInfo cinfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	u32 binding_count = 0;
	u32 resource_mask = 0;
	for (u32 i = 0; i < num_shaders; ++i)
		resource_mask |= shaders[i].resource_mask;
	for (int n = resource_mask; n != 0; n >>= 1)
		binding_count++;

	std::vector<VkDescriptorSetLayoutBinding> bindings(binding_count);

	for (u32 i = 0; i < binding_count; ++i)
	{
		for (u32 j = 0; j < num_shaders; ++j)
		{
			Shader* shader = &shaders[j];
			if ((shader->resource_mask >> i) & 1)
			{
				bindings[i].binding = i;
				bindings[i].descriptorCount = 1;
				bindings[i].descriptorType = shader->descriptor_types[i];
				bindings[i].stageFlags = shader->shader_stage;
			}
		}
	}

	cinfo.bindingCount = binding_count;
	cinfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	cinfo.pBindings = bindings.data();

	VkDescriptorSetLayout layout;
	vkCreateDescriptorSetLayout(device, &cinfo, nullptr, &layout);
	return layout;
}

Vk_Pipeline Vk_Context::create_compute_pipeline(const char* shaderpath)
{
	Shader shader;
	bool success = load_shader_from_file(&shader, device, shaderpath);
	assert(success);

	VkComputePipelineCreateInfo cinfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	
	VkPipelineShaderStageCreateInfo shader_stage_cinfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	shader_stage_cinfo.module = shader.shader;
	shader_stage_cinfo.pName = "main";
	shader_stage_cinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayout set_layout = create_descriptor_set_layout(1, &shader);

	VkPipelineLayoutCreateInfo layout_cinfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layout_cinfo.pSetLayouts = &set_layout;
	layout_cinfo.setLayoutCount = 1;
	VkPushConstantRange push_constants{};
	push_constants.offset = 0;
	push_constants.size = 128;
	push_constants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	layout_cinfo.pushConstantRangeCount = 1;
	layout_cinfo.pPushConstantRanges = &push_constants;
	VkPipelineLayout pipeline_layout;
	vkCreatePipelineLayout(device, &layout_cinfo, nullptr, &pipeline_layout);

	cinfo.layout = pipeline_layout;
	cinfo.stage = shader_stage_cinfo;

	VkPipeline pipeline;
	vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cinfo, nullptr, &pipeline);

	Vk_Pipeline pp{};
	pp.desc_sets = { set_layout };
	pp.layout = pipeline_layout;
	pp.pipeline = pipeline;
	pp.update_template = create_descriptor_update_template(1, &shader, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout);

	vkDestroyShaderModule(device, shader.shader, nullptr);
	g_garbage_collector->push([=]()
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
			vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
			vkDestroyDescriptorUpdateTemplate(device, pp.update_template, nullptr);
		}, Garbage_Collector::SHUTDOWN);

	return pp;
}

VkDescriptorSetLayout Vk_Context::create_layout_from_spirv(u8* bytecode, u32 size)
{

	SpvReflectShaderModule module;
	SpvReflectResult result = spvReflectCreateShaderModule((size_t)size, bytecode, &module);
	assert(result == SPV_REFLECT_RESULT_SUCCESS);

	u32 binding_count = 0;
	spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
	std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
	spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.data());

	u32 pc_count = 0;
	spvReflectEnumeratePushConstantBlocks(&module, &pc_count, nullptr);
	std::vector<SpvReflectBlockVariable*> push_constants(pc_count);
	spvReflectEnumeratePushConstantBlocks(&module, &pc_count, push_constants.data());

	std::vector<VkDescriptorSetLayoutBinding> vk_bindings(binding_count);
	for (u32 i = 0; i < binding_count; ++i)
	{
		VkDescriptorSetLayoutBinding b{};
		b.binding = bindings[i]->binding;
		b.descriptorCount = bindings[i]->count;
		b.descriptorType = (VkDescriptorType)(bindings[i]->descriptor_type);
		b.pImmutableSamplers = nullptr;
		b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		vk_bindings[i] = b;
	}
	VkDescriptorSetLayoutCreateInfo cinfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	cinfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	cinfo.bindingCount = (u32)vk_bindings.size();
	cinfo.pBindings = vk_bindings.data();
	VkDescriptorSetLayout layout;
	vkCreateDescriptorSetLayout(device, &cinfo, nullptr, &layout);

	return layout;
}

Vk_Allocated_Image Vk_Context::load_texture_hdri(const char* filepath, VkImageUsageFlags usage)
{
	constexpr int required_n_comps = 4;
	stbi_set_flip_vertically_on_load(1);
	int x, y, comp;
	float* data = stbi_loadf(filepath, &x, &y, &comp, required_n_comps);

	u32 required_size = (x * y * required_n_comps) * sizeof(float);
	Vk_Allocated_Image img = allocate_image(
		{ (u32)x, (u32)y, 1 }, 
		VK_FORMAT_R32G32B32A32_SFLOAT, 
		usage,
		VK_IMAGE_TILING_OPTIMAL
	);
	Vk_Allocated_Buffer staging_buffer = allocate_buffer(
		required_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	void* mapped;
	vmaMapMemory(allocator, staging_buffer.allocation, &mapped);
	memcpy(mapped, data, required_size);
	vmaUnmapMemory(allocator, staging_buffer.allocation);

	VkCommandBufferAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };

	alloc_info.commandBufferCount = 1;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(device, &alloc_info, &cmd);

	VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &begin_info);

	VkImageSubresourceLayers subres{};
	subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subres.baseArrayLayer = 0;
	subres.layerCount = 1;
	subres.mipLevel = 0;
	VkBufferImageCopy regions{};
	regions.bufferOffset = 0;
	regions.bufferImageHeight = 0;
	regions.bufferRowLength = 0;
	regions.imageSubresource = subres;
	regions.imageOffset = { 0, 0, 0 };
	regions.imageExtent = { (u32)x, (u32)y, 1 };

	vkinit::vk_transition_layout(cmd, img.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT
	);

	vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions);

	vkinit::vk_transition_layout(cmd, img.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		| VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		| VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV
	);

	vkEndCommandBuffer(cmd);

	VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;
	vkQueueSubmit(graphics_queue, 1, &submit, 0);

	vkQueueWaitIdle(graphics_queue);

	stbi_image_free(data);

	return img;
}

Raytracing_Pipeline Vk_Context::create_raytracing_pipeline(
	VkShaderModule rgen, VkShaderModule rmiss, VkShaderModule rchit,
	VkDescriptorSetLayout* layouts, int num_layouts)
{
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
	pssci[raygen_index].module = rgen;
	pssci[raygen_index].pName = "main";
	pssci[raygen_index].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	pssci[miss_index].sType = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	pssci[miss_index].module = rmiss;
	pssci[miss_index].pName = "main";
	pssci[miss_index].stage = VK_SHADER_STAGE_MISS_BIT_KHR;

	pssci[closest_hit_index].sType = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	pssci[closest_hit_index].module = rchit;
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

	VkPipelineLayout pipeline_layout;
	VkPipelineLayoutCreateInfo layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layout_create_info.pSetLayouts = layouts;
	layout_create_info.setLayoutCount = num_layouts;
	VkPushConstantRange pc_range{};
	pc_range.offset = 0;
	pc_range.size = 128;
	pc_range.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	layout_create_info.pPushConstantRanges = &pc_range;
	layout_create_info.pushConstantRangeCount = 1;
	vkCreatePipelineLayout(device, &layout_create_info, nullptr, &pipeline_layout);

	VkRayTracingPipelineCreateInfoKHR rtpci{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	rtpci.stageCount = (uint32_t)pssci.size();
	rtpci.pStages = pssci.data();
	rtpci.groupCount = (uint32_t)rtsgci.size();
	rtpci.pGroups = rtsgci.data();
	rtpci.maxPipelineRayRecursionDepth = 1;
	rtpci.layout = pipeline_layout;

	VkPipeline rt_pipeline;
	VK_CHECK(vkCreateRayTracingPipelinesKHR(
		device,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		1, &rtpci,
		nullptr,
		&rt_pipeline));

	Vk_Pipeline ret{};

	for (int i = 0; i < num_layouts; ++i)
		ret.desc_sets[i] = layouts[i];
	ret.layout = pipeline_layout;
	ret.pipeline = rt_pipeline;


	uint32_t group_count = (uint32_t)rtsgci.size();
	uint32_t group_handle_size = device_shader_group_handle_size;
	uint32_t shader_group_base_alignment = device_sbt_alignment;
	uint32_t group_size_aligned = (group_handle_size + shader_group_base_alignment - 1) & ~(shader_group_base_alignment - 1);

	uint32_t sbt_size = group_count * group_size_aligned;

	std::vector<uint8_t> shader_handle_storage(sbt_size);
	VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(
		device,
		rt_pipeline,
		0,
		group_count,
		sbt_size,
		shader_handle_storage.data()
	));

	Vk_Allocated_Buffer rt_sbt_buffer = allocate_buffer(sbt_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		VMA_MEMORY_USAGE_AUTO,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
	);

	void* mapped;
	vmaMapMemory(allocator, rt_sbt_buffer.allocation, &mapped);
	uint8_t* pdata = (uint8_t*)mapped;
	for (uint32_t g = 0; g < group_count; ++g)
	{
		memcpy(pdata, shader_handle_storage.data() + g * group_handle_size,
			group_handle_size);
		pdata += group_size_aligned;
	}
	vmaUnmapMemory(allocator, rt_sbt_buffer.allocation);

	Raytracing_Pipeline rt_pp;
	rt_pp.pipeline = ret;

	Shader_Binding_Table sbt{};
	sbt.sbt_data = rt_sbt_buffer;

	VkDeviceAddress base = get_buffer_device_address(rt_sbt_buffer);
	VkStridedDeviceAddressRegionKHR raygen_region{};
	raygen_region.deviceAddress = base + raygen_group_index * device_sbt_alignment;
	raygen_region.stride = group_handle_size;
	raygen_region.size = group_size_aligned;

	VkStridedDeviceAddressRegionKHR miss_region{};
	miss_region.deviceAddress = base + miss_group_index * device_sbt_alignment;
	miss_region.stride = group_handle_size;
	miss_region.size = group_size_aligned;

	VkStridedDeviceAddressRegionKHR hit_region{};
	hit_region.deviceAddress = base + hit_group_index * device_sbt_alignment;
	hit_region.stride = group_handle_size;
	hit_region.size = group_size_aligned;

	sbt.raygen_region = raygen_region;
	sbt.miss_region = miss_region;
	sbt.chit_region = hit_region;
	sbt.callable_region = {};

	rt_pp.shader_binding_table = sbt;

	return rt_pp;
}

Vk_Pipeline Vk_Context::create_raster_pipeline(VkShaderModule vertex_shader, VkShaderModule fragment_shader, u32 num_layouts, VkDescriptorSetLayout* layouts)
{
	Vk_Pipeline pp{};

	VkPushConstantRange pc_range{};
	pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pc_range.size = 128; // MAX 
	pc_range.offset = 0;
	VkPipelineLayoutCreateInfo layout_create_info = vkinit::pipeline_layout_create_info(num_layouts, layouts, 1, &pc_range);
	VkPipelineLayout layout;
	vkCreatePipelineLayout(device, &layout_create_info, nullptr, &layout);

	pp.layout = layout;
	
	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0] = vkinit::pipeline_shader_stage_create_info(vertex_shader, VK_SHADER_STAGE_VERTEX_BIT, "main");
	stages[1] = vkinit::pipeline_shader_stage_create_info(fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT, "main");

	VkPipelineVertexInputStateCreateInfo vertex_input = vkinit::pipeline_vertex_input_state_create_info(0, nullptr, 0, nullptr);

	VkPipelineInputAssemblyStateCreateInfo input_assembly = vkinit::pipeline_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);

	// Dynamic scissor and viewport
	VkPipelineViewportStateCreateInfo viewport_state = vkinit::pipeline_viewport_state_create_info(1, nullptr, 1, nullptr, true);

	VkPipelineRasterizationStateCreateInfo raster_state{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	raster_state.polygonMode = VK_POLYGON_MODE_FILL;
	raster_state.cullMode = VK_CULL_MODE_NONE;
	raster_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisample_state = vkinit::pipeline_multisample_state_create_info(VK_SAMPLE_COUNT_1_BIT);

	// TODO: Fill depth state

	VkPipelineColorBlendAttachmentState attachment = vkinit::pipeline_color_blend_attachment_state();
	VkPipelineColorBlendStateCreateInfo blend_state = vkinit::pipeline_color_blend_state_create_info(1, &attachment);

	VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_state = vkinit::pipeline_dynamic_state_create_info((u32)std::size(dynamic_states), dynamic_states);

	VkGraphicsPipelineCreateInfo pipeline_create_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipeline_create_info.stageCount = (u32)std::size(stages);
	pipeline_create_info.pStages = stages;
	pipeline_create_info.pVertexInputState = &vertex_input;
	pipeline_create_info.pInputAssemblyState = &input_assembly;
	pipeline_create_info.pViewportState = &viewport_state;
	pipeline_create_info.pRasterizationState = &raster_state;
	pipeline_create_info.pMultisampleState = &multisample_state;
	pipeline_create_info.pColorBlendState = &blend_state;
	pipeline_create_info.pDynamicState = &dynamic_state;
	pipeline_create_info.layout = layout;

	vkCreateGraphicsPipelines(device, 0, 1, &pipeline_create_info, nullptr, &pp.pipeline);

	return pp;
}

void Vk_Context::save_screenshot(Vk_Allocated_Image image, const char* filename)
{
}

Vk_Allocated_Image Vk_Context::load_texture(const char* filepath)
{
	constexpr int required_n_comps = 4; // GIVE ME 4 CHANNELS!!!

	stbi_set_flip_vertically_on_load(0);
	int x, y, comp;
	u8* data = stbi_load(filepath, &x, &y, &comp, required_n_comps);

	u32 required_size = (x * y * required_n_comps) * sizeof(u8);
	Vk_Allocated_Image img = allocate_image(
		{ (u32)x, (u32)y, 1 },
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_IMAGE_TILING_OPTIMAL
	);
	Vk_Allocated_Buffer staging_buffer = allocate_buffer(
		required_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	void* mapped;
	vmaMapMemory(allocator, staging_buffer.allocation, &mapped);
	memcpy(mapped, data, required_size);
	vmaUnmapMemory(allocator, staging_buffer.allocation);

	VkCommandBufferAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };

	alloc_info.commandBufferCount = 1;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(device, &alloc_info, &cmd);

	VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &begin_info);

	VkImageSubresourceLayers subres{};
	subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subres.baseArrayLayer = 0;
	subres.layerCount = 1;
	subres.mipLevel = 0;
	VkBufferImageCopy regions{};
	regions.bufferOffset = 0;
	regions.bufferImageHeight = 0;
	regions.bufferRowLength = 0;
	regions.imageSubresource = subres;
	regions.imageOffset = { 0, 0, 0 };
	regions.imageExtent = { (u32)x, (u32)y, 1 };

	vkinit::vk_transition_layout(cmd, img.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT
	);

	vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions);

	vkinit::vk_transition_layout(cmd, img.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		| VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		| VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV
	);

	vkEndCommandBuffer(cmd);

	VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;
	vkQueueSubmit(graphics_queue, 1, &submit, 0);

	vkQueueWaitIdle(graphics_queue);

	stbi_image_free(data);

	return img;
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

void GPU_Buffer::upload(VkCommandBuffer cmd, u32 frame_index)
{
	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0; // Optional
	copy_region.dstOffset = 0; // Optional
	copy_region.size = size;

	vkCmdCopyBuffer(cmd, staging_buffer[frame_index].buffer, gpu_buffer.buffer, 1, &copy_region);
}

void GPU_Buffer::update_staging_buffer(VmaAllocator allocator, u32 frame_index, void* data, size_t data_size)
{
	void* mapped;
	vmaMapMemory(allocator, staging_buffer[frame_index].allocation, &mapped);
	memcpy(mapped, data, data_size);
	vmaUnmapMemory(allocator, staging_buffer[frame_index].allocation);
}

void Garbage_Collector::push(std::function<void()> func, DESTROY_TIME timing)
{
	if (timing == END_OF_FRAME)
		end_of_frame_queue.push_back({ func, 0 });
	else if (timing == FRAMES_IN_FLIGHT)
		frames_in_flight_queue.push_back({ func, 0 });
	else if (timing == SHUTDOWN)
		on_shutdown_queue.push_back({ func, 0 });
}

void Garbage_Collector::collect()
{
	// Clear end of frame garbage
	{
		int s = (int)end_of_frame_queue.size();
		for (int i = 0; i < s; ++i)
		{
			end_of_frame_queue[i].destroy_func();
		}
		end_of_frame_queue.clear();
	}

	// Clear frames in flight garbage
	{
		int s = (int)frames_in_flight_queue.size();
		int remove_up_to = 0;
		for (int i = 0; i < s; ++i)
		{
			if (frames_in_flight_queue[i].age < 2)
				break;
			remove_up_to++;
			frames_in_flight_queue[i].destroy_func();
		}
		if (remove_up_to != 0)
			frames_in_flight_queue.erase(
				frames_in_flight_queue.begin(), 
				frames_in_flight_queue.begin() + remove_up_to
			);
	}
}

void Garbage_Collector::shutdown()
{
	// Delete these in reverse order
	int s = (int)on_shutdown_queue.size();
	for (int i = s - 1; i >= 0; --i)
		on_shutdown_queue[i].destroy_func();

	on_shutdown_queue.clear();
}

static Garbage_Collector collector;
Garbage_Collector* g_garbage_collector = &collector;