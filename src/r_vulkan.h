#pragma once 
#include "defines.h"
#include "volk/volk.h"
#include "vma/include/vk_mem_alloc.h"
#include "platform.h"

constexpr int FRAMES_IN_FLIGHT = 2;
constexpr bool USE_VALIDATION_LAYERS = true;

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

struct GPU_Buffer
{
	Vk_Allocated_Buffer gpu_buffer;
	Vk_Allocated_Buffer staging_buffer;
	size_t size;

	void upload(VkCommandBuffer cmd);
};

struct Vk_Context
{
	struct Per_Frame_Objects
	{
		VkFence fence;
		VkCommandBuffer cmd;
		VkSemaphore render_finished_sem;
		VkSemaphore image_available_sem;
	};

	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VmaAllocator allocator;
	int graphics_idx, compute_idx, transfer_idx;
	VkCommandPool command_pool;
	VkDescriptorPool descriptor_pool;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;
	uint32_t device_sbt_alignment; // Device shader binding table alignment requirement
	VkQueue graphics_queue;

	std::array<Per_Frame_Objects, FRAMES_IN_FLIGHT> frame_objects;

	// Constructor
	Vk_Context(Platform* window);

	// Functions
	void create_instance(Platform_Window* window);
	void find_physical_device();
	void init_mem_allocator();
	void create_command_pool();
	void create_swapchain(Platform* platform);
	void create_sync_objects();
	Vk_Allocated_Buffer allocate_buffer(uint32_t size,
		VkBufferUsageFlags usage, VmaMemoryUsage memory_usage, VmaAllocationCreateFlags flags, u64 alignment = 0);
	Vk_Allocated_Image allocate_image(VkExtent3D extent, VkFormat format);
	Vk_Allocated_Buffer create_buffer(VkCommandBuffer cmd, size_t size, void* data, VkBufferUsageFlags usage);
	VkDeviceAddress get_buffer_device_address(const Vk_Allocated_Buffer& buf);
	VkShaderModule create_shader_module_from_file(const char* filepath);
	VkFence create_fence();
	VkDeviceAddress get_acceleration_structure_device_address(VkAccelerationStructureKHR as);
	VkSemaphore create_semaphore();
	GPU_Buffer create_gpu_buffer(u32 size, VkBufferUsageFlags usage_flags, u32 alignment = 0);
};


