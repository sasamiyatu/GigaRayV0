#include <stdio.h>
#include <assert.h>
#include "SDL.h"
#include "SDL_vulkan.h"
#define VOLK_IMPLEMENTATION
#include "volk/volk.h"
#include "../src/vk_helpers.h"
#include "xatlas.h"

constexpr char* WINDOW_TITLE = "Lightmapper";
constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;

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

VkInstance create_instance()
{

	return 0;
}

int main(int argc, char** argv)
{
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow(
		WINDOW_TITLE,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		WIDTH, HEIGHT,
		SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN
	);

	SDL_SetRelativeMouseMode(SDL_TRUE);

	VK_CHECK(volkInitialize());



	bool quit = false;
	while (!quit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				goto shutdown;
			if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
				goto shutdown;
		}
	}
shutdown:
	assert(window);
	return 0;
}