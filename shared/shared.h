// -------------------------------------------------------------------------
//    C++ compatibility
// -------------------------------------------------------------------------

#if __cplusplus

#pragma once

// Include additional things when compiling HLSL as C++ 
// Here we use the GLM library to support HLSL types and functions
#include "glm/glm/glm.hpp"
#include "glm/glm/gtc/constants.hpp"
#include "glm/glm/gtx/compatibility.hpp"

#define OUT_PARAMETER(X) X&

using namespace glm;

inline float rsqrt(float x) { return inversesqrt(x); }
inline float saturate(float x) { return clamp(x, 0.0f, 1.0f); }

#else
#define OUT_PARAMETER(X) out X
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define input input_

#define lerp mix
#endif

struct Camera_Data
{
    mat4 view;
    mat4 proj;
    mat4 inverse_view;
    mat4 inverse_proj;
    mat4 viewproj;
    uvec4 frame_index;
};