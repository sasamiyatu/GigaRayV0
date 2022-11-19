#include "defines.h"
#include "SDL.h"
#include "volk/volk.h"

struct Platform_Window
{
	SDL_Window* window;

	void get_vulkan_window_surface_extensions(u32* count, const char** names);
};

struct Platform
{
	Platform_Window window;

	Platform();

	void init();
	void init_window(u32 w, u32 h, const char* window_title);
	void get_window_size(i32* w, i32* h);
	void create_vulkan_surface(VkInstance instance, VkSurfaceKHR* surface);
};