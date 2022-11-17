#pragma once
#include "Volk/volk.h"
#include "vma/vk_mem_alloc.h"
#define VK_NO_PROTOTYPES
#include "SDL.h"
#include "SDL_vulkan.h"
#include <vector>
#include <array>
#include <string>
#include <optional>
#include "defines.h"

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

constexpr int FRAMES_IN_FLIGHT = 2;
constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;

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

struct Vk_RenderTarget
{
	Vk_Allocated_Image image;
	VkFormat format;
	VkImageLayout layout;
	std::string name;
};

struct Vk_Framebuffer
{
	std::vector<Vk_RenderTarget> render_targets;
};


struct StagingBuffer
{
	Vk_Allocated_Buffer buffer;
	VkCommandBuffer cmd;
};

struct Vk_Context
{
	struct Per_Frame_Objects
	{
		VkFence fence;
		VkCommandBuffer cmd;
		VkSemaphore render_finished_sem;
		VkSemaphore image_available_sem;
		StagingBuffer staging_buffer;
	};

	SDL_Window* window;
	int32_t width, height;
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
	uint32_t device_sbt_alignment; // Device shader binding table alignment requirement
	VkQueue graphics_queue;

	std::array<Per_Frame_Objects, FRAMES_IN_FLIGHT> frame_objects;
};

struct Vk_Acceleration_Structure
{
	enum Level
	{
		BOTTOM_LEVEL = 0,
		TOP_LEVEL
	};
	VkAccelerationStructureKHR acceleration_structure;
	Level level;
	Vk_Allocated_Buffer acceleration_structure_buffer;
	VkDeviceAddress acceleration_structure_buffer_address;
	Vk_Allocated_Buffer scratch_buffer;
	VkDeviceAddress scratch_buffer_address;

	// Only used for TLAS
	Vk_Allocated_Buffer tlas_instances;
	VkDeviceAddress tlas_instances_address;
};

struct Mesh
{
	struct Renderer* renderer;

	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> indices;
	std::optional<Vk_Acceleration_Structure> blas;
	Vk_Allocated_Buffer vertex_buffer;
	Vk_Allocated_Buffer index_buffer;
	VkDeviceAddress vertex_buffer_address;
	VkDeviceAddress index_buffer_address;

	uint32_t get_vertex_buffer_size();
	uint32_t get_index_buffer_size();
	uint32_t get_vertex_count();
	uint32_t get_vertex_size();
	uint32_t get_primitive_count();

	void create_vertex_buffer(struct Renderer* renderer);
	void create_index_buffer(struct Renderer* renderer);

	void build_bottom_level_acceleration_structure();
};

struct Instance
{
	glm::mat4 transform;
	Mesh* mesh;
};

struct Scene
{
	std::vector<Mesh> meshes;
	std::vector<Instance> instances;
	std::optional<Vk_Acceleration_Structure> tlas;
};



struct Renderer
{
	Vk_Context context;
	bool initialized = false;

	Vk_Pipeline rt_pipeline;
	Vk_Framebuffer framebuffer;
	uint64_t frame_counter = 0;
	uint32_t swapchain_image_index = 0;

	Scene scene;

	void initialize();

	uint32_t get_frame_index() { return frame_counter % FRAMES_IN_FLIGHT; }
	const StagingBuffer& get_staging_buffer();
	VkCommandBuffer get_current_frame_command_buffer();
	void init_sdl();
	void create_vk_instance();
	void vk_find_physical_device();
	void vk_init_mem_allocator();
	void vk_create_command_pool();
	void vk_create_descriptor_set_layout();
	void vk_create_swapchain();
	void vk_create_sync_objects();
	void vk_create_render_targets();
	void vk_create_staging_buffers();
	Vk_Allocated_Buffer vk_allocate_buffer(uint32_t size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage, VmaAllocationCreateFlags flags);
	Vk_Allocated_Image vk_allocate_image(VkExtent3D extent);
	void vk_upload(Vk_Allocated_Buffer* buffer, void* data, uint32_t size);
	VkDeviceAddress get_buffer_device_address(const Vk_Allocated_Buffer& buf);
	VkShaderModule vk_create_shader_module_from_file(const char* filepath);
	VkFence vk_create_fence();
	void vk_command_buffer_single_submit(VkCommandBuffer cmd);
	VkDeviceAddress vk_get_acceleration_structure_device_address(VkAccelerationStructureKHR as);
	VkSemaphore vk_create_semaphore();
	Vk_Pipeline vk_create_rt_pipeline();
	void vk_upload_cpu_to_gpu(VkBuffer dst, void* data, uint32_t size);
	Vk_Acceleration_Structure vk_create_acceleration_structure(Mesh* mesh);
	Vk_Acceleration_Structure vk_create_top_level_acceleration_structure(Mesh* mesh);

	void begin_frame();
	void end_frame();
	void draw(struct ECS* ecs);
};

