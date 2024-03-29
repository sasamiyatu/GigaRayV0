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

struct Primitive_Info
{
    uint material_index;
    uint vertex_count;
    uint vertex_offset;
    /*uint pad;
    mat4 model;*/
};

struct Global_Constants_Data
{
    vec3 sun_direction;
    float sun_intensity;
    vec3 sun_color;
    float exposure;
    vec4 hit_dist_params;
    float unproject;
    float min_rect_dim_mul_unproject;
    float prepass_blur_radius;
    float blur_radius;
    float post_blur_radius_scale;
    int temporal_accumulation;
    int history_fix;
    int temporal_filtering_mode; // 0 = bilinear, 1 = bicubic
    float bicubic_sharpness;
    int screen_output;
    float plane_distance_sensitivity;
    vec3 camera_origin;
    uint frame_number;
    uint blur_kernel_rotation_mode;
    uint frame_num_scaling;
    uint hit_distance_scaling;
    uint use_gaussian_weight; 
    uint screen_space_sampling;
    uint use_quadratic_distribution;
    uint use_geometry_weight;
    uint use_normal_weight;
    uint use_hit_distance_weight;
    float plane_dist_norm_scale;
    float lobe_percentage;
    float hit_distance_scale;
    float stabilization_strength;
    uint use_ycocg_color_space;
    uint taa;
    uint use_probe_normal_weight;
    uint use_roughness_override;
    float roughness_override;
    float lobe_trim_factor;
    uint demodulate_specular;
    float spec_accum_base_power;
    float spec_accum_curve;
    uint indirect_diffuse;
    uint indirect_specular;
};

// Screen output defines
#define FINAL 0
#define NOISY_INPUT 1
#define DENOISED 2
#define HIT_DISTANCE 3
#define BLUR_RADIUS 4
#define HISTORY_LENGTH 5
#define NORMAL

#define PRE_BLUR_CONSTANT_ID 0
#define BLUR_CONSTANT_ID 1
#define POST_BLUR_CONSTANT_ID 2

#define BLUR_CHANNEL_DIFFUSE 0
#define BLUR_CHANNEL_SPECULAR 1