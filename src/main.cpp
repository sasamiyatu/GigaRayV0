#define VK_NO_PROTOTYPES
#include "SDL.h"
#include "SDL_vulkan.h"
#define VOLK_IMPLEMENTATION
#include "Volk/volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
#include <assert.h>
#include <vector>
#include <array>
#include "windows.h"

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			printf("Detected Vulkan error: %d\n", err);			    \
			abort();                                                \
		}                                                           \
	} while (0)

constexpr int BINDING_IMAGE_POSITION = 0;
constexpr int BINDING_IMAGE_NORMAL = 1;
constexpr int BINDING_IMAGE_AO = 2;
constexpr int BINDING_TLAS = 3;

struct Vk_Pipeline
{
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct Vk_Allocated_Buffer
{
	VkBuffer buffer;
	VmaAllocation allocation;
};

struct Vk_Allocated_Image
{
	VkImage image;
	VkImageView image_view;
	VmaAllocation allocation;
};

struct Vk_Context
{
	SDL_Window* window;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VmaAllocator allocator;
	int graphics_idx, compute_idx, transfer_idx;
	VkCommandPool command_pool;
	VkCommandBuffer main_command_buffer;
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorPool descriptor_pool;
	Vk_Pipeline rt_pipeline;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;
	Vk_Allocated_Buffer shader_binding_table;
	uint32_t aligned_size;
	VkQueue graphics_queue;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	printf("validation layer: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

void create_vk_instance(Vk_Context* context)
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
	SDL_Vulkan_GetInstanceExtensions(context->window, &count, nullptr);
	assert(count != 0);
	std::vector<const char*> instance_exts(count);
	SDL_Vulkan_GetInstanceExtensions(context->window, &count, instance_exts.data());
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

	VkDebugUtilsMessengerCreateInfoEXT ci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
	ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;// | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
	ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;// | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	ci.pfnUserCallback = debug_callback;
	VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &ci, nullptr, &context->debug_messenger));
	context->instance = instance;
}

bool check_extensions(const std::vector<const char*>& device_exts, const std::vector<VkExtensionProperties>& props)
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

void vk_find_physical_device(Vk_Context* context)
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
	vkEnumeratePhysicalDevices(context->instance, &count, nullptr);
	assert(count != 0);
	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(context->instance, &count, devices.data());

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

	context->physical_device = physical_device;

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

	context->graphics_idx = queue_indices.graphics_idx;
	context->compute_idx = queue_indices.compute_idx;
	context->transfer_idx = queue_indices.transfer_idx;

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

	context->device = dev;

	vkGetDeviceQueue(context->device, context->graphics_idx, 0, &context->graphics_queue);
}

void vk_init_mem_allocator(Vk_Context* ctx)
{
	VmaAllocator allocator;
	VmaAllocatorCreateInfo cinfo = {};
	cinfo.instance = ctx->instance;
	cinfo.device = ctx->device;
	cinfo.physicalDevice = ctx->physical_device;
	cinfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	VmaVulkanFunctions vulkan_funcs = {};
	vulkan_funcs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	vulkan_funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	cinfo.pVulkanFunctions = &vulkan_funcs;
	VK_CHECK(vmaCreateAllocator(&cinfo, &allocator));
	ctx->allocator = allocator;
}

Vk_Allocated_Buffer vk_allocate_buffer(Vk_Context* ctx, uint32_t size, VkBufferUsageFlags usage)
{
	VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = size;
	buffer_info.usage = usage;

	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	Vk_Allocated_Buffer buffer;
	VK_CHECK(vmaCreateBuffer(ctx->allocator, &buffer_info, &alloc_info, &buffer.buffer, &buffer.allocation, nullptr));

	return buffer;
}

Vk_Allocated_Image vk_allocate_image(Vk_Context* ctx, VkExtent3D extent)
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
	vmaCreateImage(ctx->allocator, &cinfo, &allocinfo, &img.image, &img.allocation, nullptr);

	VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view_info.format = cinfo.format;
	view_info.image = img.image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkCreateImageView(ctx->device, &view_info, nullptr, &img.image_view);

	return img;
}

void vk_upload(Vk_Context* ctx, Vk_Allocated_Buffer* buffer, void* data, uint32_t size)
{
	void* dst;
	vmaMapMemory(ctx->allocator, buffer->allocation, &dst);
	memcpy(dst, data, (uint32_t)size);
	vmaUnmapMemory(ctx->allocator, buffer->allocation);
}

VkDeviceAddress get_buffer_device_address(Vk_Context* ctx, const Vk_Allocated_Buffer & buf)
{
	VkBufferDeviceAddressInfo addr_info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	addr_info.buffer = buf.buffer;
	return vkGetBufferDeviceAddress(ctx->device, &addr_info);
}

void vk_create_command_pool(Vk_Context* ctx)
{
	VkCommandPoolCreateInfo pool_create_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_create_info.queueFamilyIndex = ctx->graphics_idx;
	pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK(vkCreateCommandPool(ctx->device, &pool_create_info, nullptr, &ctx->command_pool));

	VkCommandBufferAllocateInfo cmd_alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmd_alloc_info.commandPool = ctx->command_pool;
	cmd_alloc_info.commandBufferCount = 1;
	cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VK_CHECK(vkAllocateCommandBuffers(ctx->device, &cmd_alloc_info, &ctx->main_command_buffer));
}

void vk_create_descriptor_set_layout_and_pool(Vk_Context* ctx)
{
	std::array<VkDescriptorSetLayoutBinding, 4> bindings;
	bindings[0].binding = BINDING_IMAGE_POSITION;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	
	bindings[1] = bindings[0];
	bindings[1].binding = BINDING_IMAGE_NORMAL;

	bindings[2] = bindings[0];
	bindings[2].binding = BINDING_IMAGE_AO;

	bindings[3].binding = BINDING_TLAS;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_info.bindingCount = uint32_t(bindings.size());
	layout_info.pBindings = bindings.data();
	layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	VK_CHECK(vkCreateDescriptorSetLayout(ctx->device, &layout_info, nullptr, &ctx->descriptor_set_layout));

	std::array<VkDescriptorPoolSize, 2> pool_sizes;
	
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	pool_sizes[0].descriptorCount = 3;

	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	pool_sizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };	
	pool_info.maxSets = 1;
	pool_info.poolSizeCount = (uint32_t)pool_sizes.size();
	pool_info.pPoolSizes = pool_sizes.data();
	VK_CHECK(vkCreateDescriptorPool(ctx->device, &pool_info, nullptr, &ctx->descriptor_pool));
}

uint32_t read_entire_file(const char* filepath, uint8_t** data)
{
	HANDLE h = CreateFileA(filepath, FILE_GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	DWORD file_size = GetFileSize(h, 0);

	uint8_t* buffer = (uint8_t*)malloc(file_size);
	DWORD bytes_read;
	bool success = ReadFile(h, buffer, file_size, &bytes_read, 0);
	assert(success);
	assert(bytes_read == (uint32_t)file_size);

	*data = buffer;
	return (uint32_t)bytes_read;
}

VkShaderModule vk_create_shader_module_from_file(Vk_Context* ctx, const char* filepath)
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
	VK_CHECK(vkCreateShaderModule(ctx->device, &create_info, nullptr, &sm));

	return sm;
}

void vk_create_swapchain(Vk_Context* ctx)
{
	VkSwapchainCreateInfoKHR cinfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	SDL_Vulkan_CreateSurface(ctx->window, ctx->instance, &ctx->surface);

	int w, h;
	SDL_Vulkan_GetDrawableSize(ctx->window, &w, &h);
	VkExtent2D extent;
	extent.width = w; extent.height = h;
	VkSurfaceFormatKHR surface_format;
	surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	surface_format.format = VK_FORMAT_B8G8R8A8_SRGB;
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

	VkSurfaceCapabilitiesKHR surface_caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->physical_device, ctx->surface, &surface_caps);

	cinfo.surface = ctx->surface;
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

	VK_CHECK(vkCreateSwapchainKHR(ctx->device, &cinfo, nullptr, &ctx->swapchain));

	uint32_t image_count = 0;
	vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain, &image_count, nullptr);
	assert(image_count != 0);
	ctx->swapchain_images.resize(image_count);
	vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain, &image_count, ctx->swapchain_images.data());

	// Create image views
	ctx->swapchain_image_views.resize(image_count);
	for (size_t i = 0; i < ctx->swapchain_images.size(); ++i)
	{
		VkImageViewCreateInfo cinfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		cinfo.image = ctx->swapchain_images[i];
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

		VK_CHECK(vkCreateImageView(ctx->device, &cinfo, nullptr, &ctx->swapchain_image_views[i]));
	}
}

VkFence vk_create_fence(Vk_Context* ctx)
{
	VkFenceCreateInfo cinfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	cinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	
	VkFence fence;
	VK_CHECK(vkCreateFence(ctx->device, &cinfo, nullptr, &fence));

	return fence;
}

VkSemaphore vk_create_semaphore(Vk_Context* ctx)
{
	VkSemaphoreCreateInfo cinfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	
	VkSemaphore sem;
	VK_CHECK(vkCreateSemaphore(ctx->device, &cinfo, nullptr, &sem));
	return sem;
}

Vk_Pipeline vk_create_rt_pipeline(Vk_Context* ctx)
{
	VkShaderModule rgen_shader = vk_create_shader_module_from_file(ctx, "shaders/spirv/test.rgen.spv");
	VkShaderModule rmiss_shader = vk_create_shader_module_from_file(ctx, "shaders/spirv/test.rmiss.spv");
	VkShaderModule rchit_shader = vk_create_shader_module_from_file(ctx, "shaders/spirv/test.rchit.spv");

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
	cinfo.pSetLayouts = &ctx->descriptor_set_layout;
	cinfo.setLayoutCount = 1;
	VkPipelineLayout layout;
	vkCreatePipelineLayout(ctx->device, &cinfo, nullptr, &layout);

	VkRayTracingPipelineCreateInfoKHR rtpci{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	rtpci.stageCount = (uint32_t)pssci.size();
	rtpci.pStages = pssci.data();
	rtpci.groupCount = (uint32_t)rtsgci.size();
	rtpci.pGroups = rtsgci.data();
	rtpci.maxPipelineRayRecursionDepth = 4;
	rtpci.layout = layout;

	VkPipeline rt_pipeline;
	VK_CHECK(vkCreateRayTracingPipelinesKHR(
		ctx->device,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
		1, &rtpci,
		nullptr,
		&rt_pipeline));

	Vk_Pipeline pl;
	pl.layout = layout;
	pl.pipeline = rt_pipeline;

	uint32_t group_count = (uint32_t)rtsgci.size();
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
	rt_pipeline_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	VkPhysicalDeviceProperties2 device_props{};
	device_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	device_props.pNext = &rt_pipeline_props;
	vkGetPhysicalDeviceProperties2(ctx->physical_device, &device_props);
	uint32_t group_handle_size = rt_pipeline_props.shaderGroupHandleSize;
	uint32_t group_size_aligned = (group_handle_size + rt_pipeline_props.shaderGroupBaseAlignment - 1) & ~(rt_pipeline_props.shaderGroupBaseAlignment - 1);

	uint32_t sbt_size = group_count * group_size_aligned;

	std::vector<uint8_t> shader_handle_storage(sbt_size);
	VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(
		ctx->device,
		rt_pipeline,
		0,
		group_count,
		sbt_size,
		shader_handle_storage.data()
	));

	Vk_Allocated_Buffer rt_sbt_buffer = vk_allocate_buffer(ctx, sbt_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
	);
	void* mapped;
	vmaMapMemory(ctx->allocator, rt_sbt_buffer.allocation, &mapped);
	uint8_t* pdata = (uint8_t*)mapped;
	for (uint32_t g = 0; g < group_count; ++g)
	{
		memcpy(pdata, shader_handle_storage.data() + g * group_handle_size,
			group_handle_size);
		pdata += group_size_aligned;
	}
	vmaUnmapMemory(ctx->allocator, rt_sbt_buffer.allocation);

	ctx->shader_binding_table = rt_sbt_buffer;
	ctx->aligned_size = group_size_aligned;
	return pl;
}

void vk_transition_layout(
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

int main(int argc, char** argv)
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	Vk_Context vk_context;
	vk_context.window = SDL_CreateWindow("Vulkan", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);

	VkResult  res = volkInitialize();
	assert(res == VK_SUCCESS);

	create_vk_instance(&vk_context);
	vk_find_physical_device(&vk_context);
	vk_create_command_pool(&vk_context);
	vk_init_mem_allocator(&vk_context);
	vk_create_swapchain(&vk_context);
	vk_create_descriptor_set_layout_and_pool(&vk_context);
	vk_context.rt_pipeline = vk_create_rt_pipeline(&vk_context);

	VkFence fence = vk_create_fence(&vk_context);
	VkSemaphore semaphore = vk_create_semaphore(&vk_context);

	Vk_Allocated_Image img = vk_allocate_image(&vk_context, { 1280, 720, 1 });

	std::vector<float> vertices = { -0.5f, -0.5f, 0.f, 0.5f, -.5f, 0.f, 0.f, .5f, 0.f };//; 1.f, 1.f, 0.f};
	std::vector<uint32_t> indices = { 0, 1, 2 };//,2, 1, 3};

	Vk_Allocated_Buffer vertex_buffer = vk_allocate_buffer(&vk_context,
		(uint32_t)(vertices.size() * sizeof(float)),
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
	Vk_Allocated_Buffer index_buffer = vk_allocate_buffer(&vk_context,
		(uint32_t)(indices.size() * sizeof(uint32_t)),
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
	vk_upload(&vk_context, &vertex_buffer, vertices.data(), (uint32_t)(vertices.size() * sizeof(float)));
	vk_upload(&vk_context, &index_buffer, indices.data(), (uint32_t)(indices.size() * sizeof(uint32_t)));

	VkBufferDeviceAddressInfo addr_info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	addr_info.buffer = vertex_buffer.buffer;
	VkDeviceAddress vertex_buffer_addr = vkGetBufferDeviceAddress(vk_context.device, &addr_info);
	addr_info.buffer = index_buffer.buffer;
	VkDeviceAddress index_buffer_addr = vkGetBufferDeviceAddress(vk_context.device, &addr_info);

	VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vertex_buffer_addr;
	//triangles.vertexData.hostAddress = vertices.data();
	triangles.vertexStride = 3 * sizeof(float);
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = index_buffer_addr;
	//triangles.indexData.hostAddress = indices.data();
	triangles.maxVertex = vertices.size() / 3 - 1;
	triangles.transformData = { 0 };

	VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.geometry.triangles = triangles;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	VkAccelerationStructureBuildRangeInfoKHR range_info;
	range_info.firstVertex = 0;
	range_info.primitiveCount = indices.size() / 3;
	range_info.primitiveOffset = 0;
	range_info.transformOffset = 0;

	VkAccelerationStructureBuildGeometryInfoKHR build_geom_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	build_geom_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	build_geom_info.geometryCount = 1;
	build_geom_info.pGeometries = &geometry;
	build_geom_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	build_geom_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	build_geom_info.srcAccelerationStructure = VK_NULL_HANDLE;

	VkAccelerationStructureBuildSizesInfoKHR size_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(vk_context.device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_geom_info, &range_info.primitiveCount, &size_info);

	Vk_Allocated_Buffer buffer_blas = vk_allocate_buffer(&vk_context, (uint32_t)size_info.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	//uint8_t* buffer_blas_host = new uint8_t[size_info.accelerationStructureSize];
	VkAccelerationStructureKHR blas;
	VkAccelerationStructureCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	create_info.type = build_geom_info.type;
	create_info.size = size_info.accelerationStructureSize;
	create_info.buffer = buffer_blas.buffer;
	create_info.offset = 0;
	VK_CHECK(vkCreateAccelerationStructureKHR(vk_context.device, &create_info, nullptr, &blas));
	build_geom_info.dstAccelerationStructure = blas;

	Vk_Allocated_Buffer scratch_buffer = vk_allocate_buffer(&vk_context, (uint32_t)size_info.buildScratchSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	build_geom_info.scratchData.deviceAddress = get_buffer_device_address(&vk_context, scratch_buffer);

	VkAccelerationStructureBuildRangeInfoKHR* p_range_info = &range_info;
	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(vk_context.main_command_buffer, &begin_info);
	vkCmdBuildAccelerationStructuresKHR(vk_context.main_command_buffer, 1, &build_geom_info, &p_range_info);

	VkAccelerationStructureDeviceAddressInfoKHR address_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
	address_info.accelerationStructure = blas;
	VkDeviceAddress blas_address = vkGetAccelerationStructureDeviceAddressKHR(vk_context.device, &address_info);

	VkAccelerationStructureInstanceKHR instance{};
	const float rcp_sqrt_2 = sqrtf(0.5f);
	//instance.transform.matrix[0][0] = -rcp_sqrt_2;
	instance.transform.matrix[0][0] = instance.transform.matrix[1][1] = instance.transform.matrix[2][2] = 1.f;
	//instance.transform.matrix[0][2] = rcp_sqrt_2;
	//instance.transform.matrix[1][1] = 1.f;
	//instance.transform.matrix[2][0] = -rcp_sqrt_2;
	//instance.transform.matrix[2][2] = -rcp_sqrt_2;
	instance.instanceCustomIndex = 0;
	instance.mask = 0xFF;
	instance.instanceShaderBindingTableRecordOffset = 0;
	instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instance.accelerationStructureReference = blas_address;

	Vk_Allocated_Buffer buffer_instances = vk_allocate_buffer(&vk_context, (uint32_t)sizeof(instance), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	vk_upload(&vk_context, &buffer_instances, &instance, (uint32_t)sizeof(instance));
	VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = { VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR };
	vkCmdPipelineBarrier(vk_context.main_command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0,
		1, &barrier,
		0, nullptr,
		0, nullptr);

	range_info = {};
	range_info.primitiveOffset = 0;
	range_info.primitiveCount = 1;
	range_info.firstVertex = 0;
	range_info.transformOffset = 0;

	VkAccelerationStructureGeometryInstancesDataKHR instance_vk{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	instance_vk.arrayOfPointers = VK_FALSE;
	instance_vk.data.deviceAddress = get_buffer_device_address(&vk_context, buffer_instances);

	geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances = instance_vk;

	build_geom_info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	build_geom_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	build_geom_info.geometryCount = 1;
	build_geom_info.pGeometries = &geometry;
	build_geom_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	build_geom_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	build_geom_info.srcAccelerationStructure = VK_NULL_HANDLE;

	size_info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(
		vk_context.device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&build_geom_info,
		&range_info.primitiveCount,
		&size_info
	);

	Vk_Allocated_Buffer buffer_tlas = vk_allocate_buffer(&vk_context, (uint32_t)size_info.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	VkAccelerationStructureKHR tlas;
	create_info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	create_info.type = build_geom_info.type;
	create_info.size = size_info.accelerationStructureSize;
	create_info.buffer = buffer_tlas.buffer;
	create_info.offset = 0;
	VK_CHECK(vkCreateAccelerationStructureKHR(vk_context.device, &create_info, nullptr, &tlas));

	build_geom_info.dstAccelerationStructure = tlas;

	Vk_Allocated_Buffer scratch_buffer2 = vk_allocate_buffer(&vk_context, (uint32_t)size_info.buildScratchSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	build_geom_info.scratchData.deviceAddress = get_buffer_device_address(&vk_context, scratch_buffer2);

	vkCmdBuildAccelerationStructuresKHR(vk_context.main_command_buffer, 1, &build_geom_info, &p_range_info);

	vk_transition_layout(
		vk_context.main_command_buffer,
		img.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

	for (int i = 0; i < vk_context.swapchain_images.size(); ++i)
	{
		vk_transition_layout(vk_context.main_command_buffer, vk_context.swapchain_images[i],
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			0, VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
	}

	vkEndCommandBuffer(vk_context.main_command_buffer);

	VkSubmitInfo s{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	s.commandBufferCount = 1;
	s.pCommandBuffers = &vk_context.main_command_buffer;
	vkQueueSubmit(vk_context.graphics_queue, 1, &s, VK_NULL_HANDLE);
	vkDeviceWaitIdle(vk_context.device);

	bool quit = false;
	while (!quit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				quit = true;

		}
		VkCommandBuffer cmd = vk_context.main_command_buffer;
			
		VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &cmd;
		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &semaphore;

		vkWaitForFences(vk_context.device, 1, &fence, VK_TRUE, UINT64_MAX);
		vkResetFences(vk_context.device, 1, &fence);
		uint32_t image_index = 0;
		vkAcquireNextImageKHR(vk_context.device, vk_context.swapchain, UINT64_MAX, 0, fence, &image_index);
		VkImage next_image = vk_context.swapchain_images[image_index];


		vkBeginCommandBuffer(cmd, &begin_info);

		vk_transition_layout(cmd, next_image, 
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_ACCESS_SHADER_WRITE_BIT, 
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
		);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk_context.rt_pipeline.pipeline);

		VkWriteDescriptorSet writes[2] = { {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET} };
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[0].dstArrayElement = 0;
		writes[0].dstBinding = 0;
		VkDescriptorImageInfo img_info{};
		img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		img_info.imageView = img.image_view;
		writes[0].pImageInfo = &img_info;
		writes[0].dstSet = 0;

		VkWriteDescriptorSetAccelerationStructureKHR acr{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		acr.accelerationStructureCount = 1;
		acr.pAccelerationStructures = &tlas;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		writes[1].dstBinding = 3;
		writes[1].dstSet = 0;
		writes[1].pNext = &acr;
		vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk_context.rt_pipeline.layout, 0, 2, writes);

		VkDeviceAddress base = get_buffer_device_address(&vk_context, vk_context.shader_binding_table);
		VkStridedDeviceAddressRegionKHR raygen_region{};
		raygen_region.deviceAddress = base + 0 * vk_context.aligned_size;
		raygen_region.stride = 32;
		raygen_region.size = 32;

		VkStridedDeviceAddressRegionKHR miss_region{};
		miss_region.deviceAddress = base + 1 * vk_context.aligned_size;
		miss_region.stride = 32;
		miss_region.size = 32;

		VkStridedDeviceAddressRegionKHR hit_region{};
		hit_region.deviceAddress = base + 2 * vk_context.aligned_size;
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
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			1, &memory_barrier,
			0, nullptr,
			0, nullptr
		);

		VkImageBlit region;
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


		vkCmdBlitImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL, next_image, VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_LINEAR);
		vk_transition_layout(
			cmd, next_image,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			0, 0,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
		);
		vkEndCommandBuffer(cmd);

		VkPresentInfoKHR present_info{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		present_info.swapchainCount = 1;
		present_info.pSwapchains = &vk_context.swapchain;
		present_info.pImageIndices = &image_index;


		vkQueueSubmit(vk_context.graphics_queue, 1, &submit, VK_NULL_HANDLE);

		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = &semaphore;
		VK_CHECK(vkQueuePresentKHR(vk_context.graphics_queue, &present_info));
		vkDeviceWaitIdle(vk_context.device);
	}
	
	return 0;
}