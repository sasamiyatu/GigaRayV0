#pragma once
#include "r_vulkan.h"

template <typename T>
struct Jeeves
{
	Vk_Context* ctx;
	T resource;
	Jeeves(T resource, Vk_Context* ctx)
		: resource(resource), ctx(ctx)
	{
	}

	~Jeeves()
	{
		if constexpr (std::is_same<T, Vk_Allocated_Image>::value)
		{
			g_garbage_collector->push([=]()
				{
					vmaDestroyImage(ctx->allocator, resource.image, resource.allocation);
				}, Garbage_Collector::FRAMES_IN_FLIGHT);
		}
	}
};