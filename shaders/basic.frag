#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#include "math.glsl"
#include "scene.glsl"
#include "brdf.h"
#include "sh.glsl"

layout(set = 0, binding = 0) uniform sampler2D brdf_lut;
layout(set = 0, binding = 1) uniform sampler2D prefiltered_envmap;
layout(set = 0, binding = 5) buffer SH_sample_buffer
{
    SH_3 samples[];
} SH_samples;

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec3 base_color;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in vec3 frag_pos;
layout(location = 4) in vec3 camera_pos;
layout(location = 5) flat in float roughness;
layout (location = 6) flat in Material mat;

const vec3 SILVER = vec3(0.95, 0.93, 0.88);

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   

vec3 eval_sh(SH_3 sh, vec3 n)
{
    vec3 sh_result[9];
    sh_result[0] = 0.282095f * sh.coefs[0];
    sh_result[1] = -0.488603f * n.y * sh.coefs[1];
    sh_result[2] = 0.488603f * n.z * sh.coefs[2];
    sh_result[3] = -0.488603f * n.x * sh.coefs[3];
    sh_result[4] = 1.092548f * n.x * n.y * sh.coefs[4];
    sh_result[5] = -1.092548f * n.y * n.z * sh.coefs[5];
    sh_result[6] = 0.315392f * (3.0f * n.z * n.z - 1.0f) * sh.coefs[6];
    sh_result[7] = -1.092548f * n.x * n.z * sh.coefs[7];
    sh_result[8] = 0.54627f * (n.x * n.x - n.y * n.y) * sh.coefs[8];

    vec3 col = vec3(0.0);
    for (int i = 0; i < 9; ++i)
    {
        col += sh_result[i];
    }
    col = max(vec3(0.0), col);
    return col;
}

layout(location = 0) out vec4 color;
void main()
{
    vec3 normal = normalize(in_normal);
    //color = vec4(normal * 0.5 + 0.5, 1.0);
    vec3 N = normal;
    vec3 V = normalize(camera_pos - frag_pos);
    vec3 R = reflect(-V, normal);
    vec3 envmap_sample = texture(envmap_cube, R).rgb;
    vec2 uv = equirectangular_to_uv(R);
    float lod = roughness * 4.0;
    vec3 env = textureLod(prefiltered_envmap, uv, lod).rgb;
    float NoV = max(0.0, dot(normal, V));
    // vec4 t = texture(brdf_lut, vec2(NoV, roughness));
    // vec4 t3 = texture(brdf_lut, texcoord);
    // vec2 t2 = texture(brdf_lut, vec2(NoV, 0.01)).rg;

    vec4 base_color = mat.base_color_tex != -1 ? texture(textures[mat.base_color_tex], texcoord, 0) : mat.base_color_factor;
    if (base_color.a < 0.5) discard;
    vec3 metallic_roughness = texture(textures[mat.metallic_roughness_tex], texcoord).rgb;
    vec3 albedo = pow(base_color.rgb, vec3(2.2));
    float metallic = metallic_roughness.b;
    float alpha = metallic_roughness.g * metallic_roughness.g;
    float alpha2 = alpha * alpha;
    vec3 L = normalize(vec3(0.0, 1.0, 0.2));
    vec3 H = normalize(L + V);
    float NoL = max(0.0, dot(N, L));
    float NoH = max(0.0, dot(N, H));
    float HoV = max(0.0, dot(H, V));
    float HoL = max(0.0, dot(H, L));

    vec3 specular_color = mix(vec3(0.04), albedo, metallic);
    vec3 diffuse = mix(albedo, vec3(0.0), metallic);
    float G_vis = Smith_G2_Height_Correlated_GGX_Lagarde(alpha2, NoL, NoV);
    vec3 F = evalFresnelSchlick(specular_color, 1.0, HoL);
    float D =  GGX_D(alpha2, NoH);

    vec3 kS = G_vis * F * D;
    vec3 kD = (1.0 - F) * diffuse / PI;

    MaterialProperties mat_props;
    mat_props.baseColor = albedo;
    mat_props.metalness = metallic_roughness.b;
    mat_props.roughness = metallic_roughness.g;
    mat_props.emissive = vec3(0.0);
    mat_props.transmissivness = 0.0;
    mat_props.opacity = 0.0;
    vec3 brdf = evalCombinedBRDF(N, L, V, mat_props);
    //vec3 total = (kS + kD) * NoL;
    vec3 total = brdf;
    //ckRoughness(NdotV, SILVER, roughness);
    //vec3 specular = env * (specular_color * t.x + t.y);
    color = vec4(total, 1.0);
    color = vec4(envmap_sample, 1.0);
    SH_3 sh = SH_samples.samples[0];
    vec3 evaluated_sh = eval_sh(sh, N);
    color = vec4(evaluated_sh, 1.0);
}