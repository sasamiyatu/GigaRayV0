#ifndef MATERIAL_H
#define MATERIAL_H

// GPU material struct
struct Material 
{
    int base_color_tex;
    int metallic_roughness_tex;
    int normal_tex;

    vec4 base_color_factor;
    float metallic_factor;
    float roughness_factor;
};

// Shading material info
struct Material_Properties
{
    vec3 base_color;
    float metallic;
    vec3 emissive;
    float roughness;
};









#endif