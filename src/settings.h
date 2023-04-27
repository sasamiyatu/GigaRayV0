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
    float exposure = 1.0f;
    float sun_azimuth = glm::degrees(3.14159f);
    float sun_zenith = glm::degrees(0.1974f);
    float sun_intensity = 1.0f;
    glm::vec3 sun_color = glm::vec3(1.0f);
    glm::vec4 hit_distance_params = glm::vec4(3.0f, 0.1f, 20.0f, 25.0f);
    float prepass_blur_radius = 15.0f;
    float blur_radius = 7.5f;
    bool temporal_accumulation = true;
    bool history_fix = true;
    int screen_output = DENOISED; // Screen_Output
    Temporal_Filtering_Mode temporal_filter = Temporal_Filtering_Mode::BILINEAR;
    float bicubic_sharpness = 0.5f;
};

extern Settings g_settings;