#version 460
#extension GL_GOOGLE_include_directive : enable
#include "math.glsl"

layout(set = 0, binding = 0) uniform sampler2D brdf_lut;
layout(set = 0, binding = 1) uniform sampler2D prefiltered_envmap;

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec3 base_color;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in vec3 frag_pos;
layout(location = 4) in vec3 camera_pos;
layout(location = 5) flat in float roughness;

const vec3 SILVER = vec3(0.95, 0.93, 0.88);

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   

layout(location = 0) out vec4 color;
void main()
{
    vec3 normal = normalize(in_normal);
    //color = vec4(normal * 0.5 + 0.5, 1.0);

    vec3 V = normalize(camera_pos - frag_pos);
    vec3 R = reflect(-V, normal);
    vec2 uv = equirectangular_to_uv(R);
    float lod = roughness * 4.0;
    vec3 env = textureLod(prefiltered_envmap, uv, lod).rgb;
    float NdotV = max(0.0, dot(normal, V));
    vec4 t = texture(brdf_lut, vec2(NdotV, roughness));
    vec4 t3 = texture(brdf_lut, texcoord);
    vec2 t2 = texture(brdf_lut, vec2(NdotV, 0.01)).rg;
    vec3 specular_color = fresnelSchlickRoughness(NdotV, SILVER, roughness);
    vec3 specular = env * (specular_color * t.x + t.y);
    color = vec4(normal * 0.5 + 0.5, 1.0);
}