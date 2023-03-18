#version 460

#extension GL_GOOGLE_include_directive : enable

#include "scene.glsl"

layout(location=0) in vec3 frag_pos;

layout(location = 0) out vec4 color;

layout (set = 0, binding = 0) uniform samplerCube cubemap_sampler;

const vec2 inv_atan = vec2(0.1591, 0.3183);

vec2 sample_equirectangular_map(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= inv_atan;
    uv += 0.5;
    return uv;
}

void main()
{
    vec3 dir = normalize(frag_pos);
    vec2 uv = sample_equirectangular_map(dir);
    vec3 radiance = texture(environment_map, uv).rgb;
    vec3 cube_radiance = texture(cubemap_sampler, dir).rgb;
    color = vec4(cube_radiance, 1.0);
    //color = vec4(1.0, 0.0, 1.0, 1.0);
}