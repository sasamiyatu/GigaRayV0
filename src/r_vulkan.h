#pragma once 
#include "defines.h"
#include "volk/volk.h"
#include "vma/include/vk_mem_alloc.h"
#include "platform.h"
#include "spirv-reflect/spirv_reflect.h"


constexpr int FRAMES_IN_FLIGHT = 2;
#ifdef _DEBUG
constexpr bool USE_VALIDATION_LAYERS = true;
#else
constexpr bool USE_VALIDATION_LAYERS = false;
#endif


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
	std::array<Vk_Allocated_Buffer, FRAMES_IN_FLIGHT> staging_buffer;
	size_t size;

	void upload(VkCommandBuffer cmd, u32 frame_index);
	void update_staging_buffer(VmaAllocator allocator, u32 frame_index, void* data, size_t data_size);
};

struct Vk_Pipeline
{
	VkPipeline pipeline;
	VkPipelineLayout layout;
	std::array<VkDescriptorSetLayout, 4> desc_sets;
	VkDescriptorUpdateTemplate update_template; // Push descriptor update template
};


struct Shader_Binding_Table
{
	Vk_Allocated_Buffer sbt_data;
	VkStridedDeviceAddressRegionKHR raygen_region;
	VkStridedDeviceAddressRegionKHR miss_region;
	VkStridedDeviceAddressRegionKHR chit_region;
	VkStridedDeviceAddressRegionKHR callable_region;
};

struct Raytracing_Pipeline
{
	Vk_Pipeline pipeline;
	Shader_Binding_Table shader_binding_table;
};

struct Garbage_Collector
{
	struct Garbage
	{
		std::function<void()> destroy_func;
		u32 age;
	};

	enum DESTROY_TIME
	{
		END_OF_FRAME = 0,
		FRAMES_IN_FLIGHT,
		SHUTDOWN
	};

	std::vector<Garbage> end_of_frame_queue;
	std::vector<Garbage> frames_in_flight_queue;
	std::vector<Garbage> on_shutdown_queue;

	void push(std::function<void()> func, DESTROY_TIME timing);
	void collect();
	void shutdown();
};

extern Garbage_Collector* g_garbage_collector;

struct Async_Upload
{
	VkQueue upload_queue;
	VkSemaphore timeline_sem;
	u64 timeline_semaphore_value;
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
	VkCommandPool async_command_pool;
	VkDescriptorPool descriptor_pool;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;
	uint32_t device_sbt_alignment; // Device shader binding table alignment requirement
	uint32_t device_shader_group_handle_size;
	VkQueue graphics_queue;
	Async_Upload async_upload;
	VkPhysicalDeviceProperties2 physical_device_properties;

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
	VkQueryPool create_query_pool();
	Vk_Allocated_Buffer allocate_buffer(uint32_t size,
		VkBufferUsageFlags usage, VmaMemoryUsage memory_usage, VmaAllocationCreateFlags flags, u64 alignment = 0);
	Vk_Allocated_Image allocate_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL, int mip_levels = 1);
	void free_image(Vk_Allocated_Image img);
	void free_buffer(Vk_Allocated_Buffer buffer);
	Vk_Allocated_Buffer create_buffer(VkCommandBuffer cmd, size_t size, void* data, VkBufferUsageFlags usage);
	VkDeviceAddress get_buffer_device_address(const Vk_Allocated_Buffer& buf);
	VkShaderModule create_shader_module_from_file(const char* filepath);
	VkShaderModule create_shader_module_from_bytes(u32* bytes, size_t size);
	VkDescriptorUpdateTemplate Vk_Context::create_descriptor_update_template(struct Shader* shader, VkPipelineBindPoint bind_point, VkPipelineLayout pipeline_layout);
	VkFence create_fence();
	VkDeviceAddress get_acceleration_structure_device_address(VkAccelerationStructureKHR as);
	VkSemaphore create_semaphore(bool timeline = false);
	GPU_Buffer create_gpu_buffer(u32 size, VkBufferUsageFlags usage_flags, u32 alignment = 0);
	Vk_Pipeline create_compute_pipeline(const char* shaderpath);
	VkDescriptorSetLayout create_layout_from_spirv(u8* bytecode, u32 size);
	Vk_Allocated_Image load_texture_hdri(const char* filepath, VkImageUsageFlags usage = (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
	Vk_Allocated_Image load_texture(const char* filepath);
	Vk_Allocated_Image load_texture_async(const char* filepath, u64* timeline_semaphore_value);
	VkDescriptorSetLayout create_descriptor_set_layout(struct Shader* shader);
	Raytracing_Pipeline create_raytracing_pipeline(
		VkShaderModule rgen, VkShaderModule rmiss, VkShaderModule rchit,
		VkDescriptorSetLayout* layouts, int num_layouts);

	void save_screenshot(Vk_Allocated_Image image, const char* filename);
};


