#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_query : require
#extension GL_EXT_debug_printf : require
#include "math.glsl"
#include "scene.glsl"
#include "brdf.h"
#include "sh.glsl"
#include "random.glsl"

layout(set = 0, binding = 0) uniform sampler2D brdf_lut;
layout(set = 0, binding = 1) uniform sampler2D prefiltered_envmap;
layout(set = 0, binding = 5, scalar) buffer SH_sample_buffer
{
    SH_2 probes[];
} SH_probes;
layout(set = 0, binding = 6) uniform accelerationStructureEXT scene;

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec3 base_color;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in vec3 frag_pos;
layout(location = 4) in vec3 camera_pos;
layout(location = 5) flat in float roughness;
layout(location = 6) in vec3 view_z;
layout(location = 7) flat in Material mat;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 normal_roughness;
layout(location = 2) out vec4 basecolor_metalness;
layout(location = 3) out vec4 world_position;

const vec3 SILVER = vec3(0.95, 0.93, 0.88);

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

layout( push_constant ) uniform constants
{
    uvec3 probe_counts;
    float probe_spacing;
    vec3 probe_min;
} control;

uint get_probe_linear_index(ivec3 p)
{
    return control.probe_counts.x * control.probe_counts.y * p.z + 
        control.probe_counts.x * p.y + p.x;
}

vec3 get_probe_irradiance(vec3 P, vec3 N)
{
    vec3 pos_relative = (P - control.probe_min) / control.probe_spacing;
    ivec3 nearest = ivec3(round(pos_relative));
    ivec3 lower = ivec3(floor(pos_relative));

    vec3 t = fract(pos_relative);
#if 0
    SH_2 interpolated_x[4];
    // Interpolate along the X axis first
    // This reduced the 3-dimensional 2x2x2 probe volume to a 2-dimensions that we can then bilinearly interpolate
    for (int y = 0; y < 2; ++y)
    {
        for (int z = 0; z < 2; ++z)
        {
            ivec3 p0 = lower + ivec3(0, y, z);
            ivec3 p1 = lower + ivec3(1, y, z);
            SH_2 probe0 = SH_probes.probes[get_probe_linear_index(p0)];
            SH_2 probe1 = SH_probes.probes[get_probe_linear_index(p1)];
            SH_2 result;
            for (int i = 0; i < 9; ++i)
            {
                result.coefs[i] = (1.0 - t.x) * probe0.coefs[i] + t.x * probe1.coefs[i];
            }
            interpolated_x[y * 2 + z] = result;
        }
    }

    SH_2 interpolated_y[2];
    // Interpolate along y 
    for (int z = 0; z < 2; ++z)
    {
        SH_2 probe0 = interpolated_x[z];
        SH_2 probe1 = interpolated_x[2 + z];
        SH_2 result;
        for (int i = 0; i < 9; ++i)
        {
            result.coefs[i] = (1.0 - t.y) * probe0.coefs[i] + t.y * probe1.coefs[i];
        }
        interpolated_y[z] = result;
    }

    // Finally along z axis
    SH_2 result;
    for (int i = 0; i < 9; ++i)
    {
        result.coefs[i] = (1.0 - t.z) * interpolated_y[0].coefs[i] + t.z * interpolated_y[1].coefs[i];
    }
#else

    float w_total = 0.0;
    SH_2 result;

    for (int i = 0; i < 9; ++i)
        result.coefs[i] = vec3(0.0);

    for (int z = 0; z < 2; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                vec3 steps = step(vec3(0.5), vec3(x, y, z));
                steps = steps * -2.0 + 1.0;
                vec3 weights = steps * (0.5 - t) + 0.5;
                float w = weights.x * weights.y * weights.z;

                vec3 probe_pos = (lower + ivec3(x, y, z)) * control.probe_spacing + control.probe_min;
                vec3 dir = probe_pos - P;
                float dist = length(dir);
                dir /= dist;

                float vis = 1.0;

#if 1
                rayQueryEXT rq;
                rayQueryInitializeEXT(rq, scene, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, P + N * 0.001, 0.0, dir, dist);
                rayQueryProceedEXT(rq);
                if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) 
                    vis = 0.0;
#endif

                w *= vis;

                SH_2 probe = SH_probes.probes[get_probe_linear_index(lower + ivec3(x, y, z))];
                w_total += w;

                for (int i = 0; i < 9; ++i)
                    result.coefs[i] += w * probe.coefs[i];
            }
        }
    }

    for (int i = 0; i < 9; ++i)
        result.coefs[i] /= w_total;
#endif

    uint probe_linear_index = control.probe_counts.x * control.probe_counts.y * nearest.z + 
    control.probe_counts.x * nearest.y + nearest.x;
    SH_2 probe = SH_probes.probes[probe_linear_index];

    vec3 evaluated_sh = eval_sh(result, N);


    return evaluated_sh;
}
void main()
{
    vec3 normal = normalize(in_normal);
    //color = vec4(normal * 0.5 + 0.5, 1.0);
    vec3 N = normal;
    vec3 V = normalize(camera_pos - frag_pos);
    vec3 R = reflect(-V, normal);

    vec3 L = normalize(vec3(0.0, 1.0, 0.2));

    float shadow = 1.0;
#if 0
    rayQueryEXT rq;

    rayQueryInitializeEXT(rq, scene, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, frag_pos + normal * 0.001, 0.0, L, 10000.0);

    rayQueryProceedEXT(rq);

    if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) 
    {
        shadow = 0.0;
    }   
#endif
    vec3 envmap_sample = texture(envmap_cube, R).rgb;
    vec2 uv = equirectangular_to_uv(R);
    float lod = roughness * 4.0;
    vec3 env = textureLod(prefiltered_envmap, uv, lod).rgb;
    float NoV = max(0.0, dot(normal, V));
    // vec4 t = texture(brdf_lut, vec2(NoV, roughness));
    // vec4 t3 = texture(brdf_lut, texcoord);
    // vec2 t2 = texture(brdf_lut, vec2(NoV, 0.01)).rg;

#if 0
    vec3 evaluated_sh = get_probe_irradiance(frag_pos, N);
#else
    vec3 evaluated_sh = vec3(0.0);
#endif

    vec4 base_color = mat.base_color_tex != -1 ? texture(textures[mat.base_color_tex], texcoord) : mat.base_color_factor;
    if (base_color.a < 0.5) discard;
    vec3 metallic_roughness = texture(textures[mat.metallic_roughness_tex], texcoord).rgb;
    vec3 albedo = pow(base_color.rgb, vec3(2.2));
    float metallic = metallic_roughness.b;
    float alpha = metallic_roughness.g * metallic_roughness.g;
    float alpha2 = alpha * alpha;

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

#if 0
    if (gl_FragCoord.xy == vec2(640.5, 360.5))
    {
        debugPrintfEXT("frag: %f %f %f", frag_pos.x, frag_pos.y, frag_pos.z);
    }
#endif

    MaterialProperties mat_props;
    mat_props.baseColor = albedo;
    mat_props.metalness = metallic_roughness.b;
    mat_props.roughness = metallic_roughness.g;
    mat_props.emissive = vec3(0.0);
    mat_props.transmissivness = 0.0;
    mat_props.opacity = 0.0;
    const float light_intensity = 100.0f;
    vec3 brdf = evalCombinedBRDF(N, L, V, mat_props) * light_intensity;
    //vec3 total = (kS + kD) * NoL;
    vec3 total = brdf * shadow;
    vec3 indirect = evaluated_sh * diffuse;
    //ckRoughness(NdotV, SILVER, roughness);
    //vec3 specular = env * (specular_color * t.x + t.y);
    //color = vec4(total + indirect, 1.0);
    color = vec4(total, base_color.a);
    //color = vec4(evaluated_sh, 1.0);
    normal_roughness = vec4(encode_unit_vector(N, false), mat_props.roughness, 1.0);
    basecolor_metalness = vec4(mat_props.baseColor.rgb, mat_props.metalness);
    // if (gl_FragCoord.xy == vec2(0.5))
    // {
    //     debugPrintfEXT("base_color: %f %f %f", mat_props.baseColor.x, mat_props.baseColor.y, mat_props.baseColor.z);
    // }
    world_position = vec4(frag_pos, 1.0);

    // color = vec4(envmap_sample, 1.0);
    // SH_2 sh = SH_samples.samples[0];
    // vec3 evaluated_sh = eval_sh(sh, N);
    // color = vec4(evaluated_sh, 1.0);
}