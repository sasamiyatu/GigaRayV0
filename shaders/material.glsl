#ifndef MATERIAL_H
#define MATERIAL_H

struct Material 
{
    int base_color_tex;
    int metallic_roughness_tex;
    int normal_tex;

    vec3 base_color_factor;
    float metallic_factor;
    float roughness_factor;
};











#endif