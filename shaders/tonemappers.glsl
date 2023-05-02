#ifndef TONEMAPPERS_GLSL
#define TONEMAPPERS_GLSL

vec3 tonemap_reinhardt(vec3 color)
{
    return color / (color + 1.0);
}















#endif