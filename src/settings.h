#pragma once
#include "defines.h"
#include "../shared/shared.h"
//#include "EnumBuilder.h"

//BEGIN_ENUM(Rendering_Mode)
//{
//    DECL_ENUM_ELEMENT(REFERENCE_PATH_TRACER),
//    DECL_ENUM_ELEMENT(HYBRID_RENDERER),
//    DECL_ENUM_ELEMENT(MAX_RENDER_MODES)
//} END_ENUM(Rendering_Mode)

 enum class Rendering_Mode
 {
     REFERENCE_PATH_TRACER = 0,
     HYBRID_RENDERER,
     MAX_RENDER_MODES
 };

 enum class Temporal_Filtering_Mode
 {
     BILINEAR = 0,
     BICUBIC,
     COUNT
 };

struct Settings
{
    Rendering_Mode rendering_mode = Rendering_Mode::REFERENCE_PATH_TRACER;
    bool menu_open = false;
    float camera_fov = 75.0f;
    float exposure = 5.0f;
    float sun_azimuth = glm::degrees(3.14159f);
    float sun_zenith = glm::degrees(0.1974f);
    float sun_intensity = 20.0f;
    glm::vec3 sun_color = glm::vec3(1.0f);
    glm::vec4 hit_distance_params = glm::vec4(3.0f, 0.1f, 20.0f, -25.0f);
    float prepass_blur_radius = 15.0f;
    float blur_radius = 7.5f;
    float post_blur_radius_scale = 2.0f;
    bool temporal_accumulation = true;
    bool history_fix = true;
    int screen_output = 0; // Screen_Output
    Temporal_Filtering_Mode temporal_filter = Temporal_Filtering_Mode::BICUBIC;
    float bicubic_sharpness = 0.5f;
    bool animate_noise = true;
    float plane_dist_sensitivity = 0.5f; // percentage
    float occlusion_threshold = 0.005f;
    int blur_kernel_rotation_mode = 1; // 0 = None, 1 = Per frame, 2 = Per pixel
    bool frame_num_scaling = true;
    bool hit_dist_scaling = true;
    bool use_gaussian_weight = true;
    bool screen_space_sampling = false;
    bool use_quadratic_distribution = false;
    bool use_geometry_weight = true;
    bool use_normal_weight = true;
    bool use_hit_distance_weight = true;
    float plane_dist_norm_scale = 1.0f;
    float lobe_percentage = 0.14f;
    float hit_distance_scale = 1.0f;
    float stabilization_strength = 1.0;
    bool use_ycocg_color_space = false;
    bool use_alternative_history_fix = true;
    float history_fix_stride = 14.0f;
    bool taa = false;
    bool jitter = false;
    bool visualize_probes = false;
    bool use_probe_normal_weight = false;
    bool use_roughness_override = false;
    float roughness_override = 0.5f;
    float lobe_trim_factor = 1.0f;
    bool demodulate_specular = true;
    float spec_accum_base_power = 0.5f;
    float spec_accum_curve = 1.0;
    bool indirect_diffuse = true;
    bool indirect_specular = false;
};

extern Settings g_settings;