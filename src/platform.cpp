#include "platform.h"
#include "SDL_vulkan.h"

void Platform_Window::get_vulkan_window_surface_extensions(u32* count, const char** names)
{
	SDL_Vulkan_GetInstanceExtensions(window, count, names);
}

Platform::Platform()
{
	init();
}

void Platform::init()
{
	SDL_InitSubSystem(SDL_INIT_EVENTS);
}

void Platform::init_window(u32 w, u32 h, const char* window_title)
{
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	window.window = SDL_CreateWindow(
		window_title,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		w, h,
		SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN
	);

	SDL_SetRelativeMouseMode(SDL_TRUE);
	assert(window.window != nullptr);
}

void Platform::get_window_size(i32* w, i32* h)
{
	SDL_Vulkan_GetDrawableSize(window.window, w, h);
}

void Platform::create_vulkan_surface(VkInstance instance, VkSurfaceKHR* surface)
{
	SDL_Vulkan_CreateSurface(window.window, instance, surface);
}
