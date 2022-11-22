#include "r_vulkan.h"
#include "common.h"

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

	volkLoadInstance(instance);

	if constexpr (USE_VALIDATION_LAYERS)
	{
		VkDebugUtilsMessengerCreateInfoEXT ci{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;// | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
		ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;// | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		ci.pfnUserCallback = debug_callback;
		VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &ci, nullptr, &debug_messenger));
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

	graphics_idx = queue_indices.graphics_idx;
	compute_idx = queue_indices.compute_idx;
	transfer_idx = queue_indices.transfer_idx;

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

	device = dev;

	vkGetDeviceQueue(device, graphics_idx, 0, &graphics_queue);
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
}

void Vk_Context::create_command_pool()
{
	VkCommandPoolCreateInfo pool_create_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_create_info.queueFamilyIndex = graphics_idx;
	pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK(vkCreateCommandPool(device, &pool_create_info, nullptr, &command_pool));

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
			f.format == VK_FORMAT_B8G8R8A8_SRGB)
		{
			return f;
		}
	}

	return formats[0];
}

static VkPresentModeKHR get_present_mode(const std::vector<VkPresentModeKHR>& present_modes)
{
	for (const auto& m : present_modes)
	{
		if (m == VK_PRESENT_MODE_MAILBOX_KHR)
			return m;
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

void Vk_Context::create_swapchain(Platform* platform)
{
	VkSwapchainCreateInfoKHR cinfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };

	platform->create_vulkan_surface(instance, &surface);
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
	}
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

	return buffer;
}

Vk_Allocated_Image Vk_Context::allocate_image(VkExtent3D extent, VkFormat format)
{
	VkImageCreateInfo cinfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	cinfo.arrayLayers = 1;
	cinfo.extent = extent;
	cinfo.format = format;
	cinfo.imageType = extent.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
	cinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	cinfo.mipLevels = 1;
	cinfo.samples = VK_SAMPLE_COUNT_1_BIT;
	cinfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo allocinfo{};
	allocinfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	Vk_Allocated_Image img;
	vmaCreateImage(allocator, &cinfo, &allocinfo, &img.image, &img.allocation, nullptr);

	VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view_info.format = cinfo.format;
	view_info.image = img.image;
	view_info.viewType = extent.depth == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkCreateImageView(device, &view_info, nullptr, &img.image_view);

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

	return sm;
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

VkSemaphore Vk_Context::create_semaphore()
{
	VkSemaphoreCreateInfo cinfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkSemaphore sem;
	VK_CHECK(vkCreateSemaphore(device, &cinfo, nullptr, &sem));
	return sem;
}

GPU_Buffer Vk_Context::create_gpu_buffer(u32 size, VkBufferUsageFlags usage_flags, u32 alignment)
{
	GPU_Buffer buffer{};

	buffer.size = (size_t)size;
	buffer.gpu_buffer = allocate_buffer(size, usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO, 0, alignment);
	buffer.staging_buffer = allocate_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, alignment);

	return buffer;
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

void GPU_Buffer::upload(VkCommandBuffer cmd)
{
	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0; // Optional
	copy_region.dstOffset = 0; // Optional
	copy_region.size = size;

	vkCmdCopyBuffer(cmd, staging_buffer.buffer, gpu_buffer.buffer, 1, &copy_region);
}
