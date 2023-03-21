#pragma once

// Defines and files included everywhere
#include <assert.h>
#include <stdint.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <optional>
#include <functional>
#include <array>
#include <map>
//#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

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

#if _WIN32
#define NOMINMAX
#include "windows.h"
#endif

#define GB_VALIDATE(expr, str) \
	do                         \
	{                          \
		if (!expr)             \
		{                      \
			printf(str);       \
			__debugbreak();    \
		}                      \
	} while (0)

constexpr char* APP_NAME = "GigaRayV0";
constexpr char* ENGINE_NAME = "GigaEngine";

typedef uint64_t u64;
typedef int64_t i64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint8_t u8;
typedef int8_t i8;
typedef float f32;
typedef double f64;



