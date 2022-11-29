#ifndef GGX_H
#define GGX_H
#include "math.glsl"
vec3 f_schlick(vec3 f0, vec3 f90, float u)
{
    return f0 + (f90 - f0) * pow(1.f - u, 5.f);
}

float v_smith_ggx_correlated(float ndotl, float ndotv, float alpha)
{
    float a2 = alpha * alpha;
    float lambda_ggxv = ndotl * sqrt((-ndotv * a2 + ndotv) * ndotv + a2);
    float lambda_ggxl = ndotv * sqrt((-ndotl * a2 + ndotl) * ndotl + a2);

    return 0.5f / (lambda_ggxv + lambda_ggxl + 0.001);
}

// Also known as G1
float ggx_masking(float v_dot_n, float v_dot_h, float a)
{
    float a2 = a*a;
    return step(0.0f, v_dot_n * v_dot_h) * 2.0f /
        (1.0f + sqrt(1.0f + a2 / (v_dot_n * v_dot_n) - a2));
}

// Also known as G
float ggx_masking_shadowing(
    float v_dot_n, float v_dot_h, float l_dot_n, float l_dot_h, float a
){
    float a2 = a*a;
    return step(0.0f, v_dot_n * v_dot_h) * step(0.0f, l_dot_n * l_dot_h) * 4.0f /
        ((1.0f + sqrt(1.0f + a2 / max(v_dot_n * v_dot_n, 1e-18) - a2)) *
         (1.0f + sqrt(1.0f + a2 / max(l_dot_n * l_dot_n, 1e-18) - a2)));
}

float d_ggx(float ndoth, float alpha)
{
    float a2 = alpha * alpha;
    float f = (ndoth * a2 - ndoth) * ndoth + 1.0;
    return a2 / (M_PI * f * f); 
}

vec3 brdf_ggx_spec(vec3 f0, vec3 f90, float ldoth, float ndotv, float ndotl, float ndoth, float roughness)
{
    vec3 F = f_schlick(f0, f90, ldoth);
    float Vis = v_smith_ggx_correlated(ndotv, ndotl, roughness);
    float D = d_ggx(ndoth, roughness);
    vec3 Fr = D * F * Vis;
    return Fr;
}


vec3 brdf_ggx_combined(float ldoth, float ndotv, float ndotl, float ndoth, float metallic, float roughness, vec3 albedo)
{
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = f_schlick(f0, vec3(1.0), ldoth);
    float Vis = v_smith_ggx_correlated(ndotl, ndotv, roughness);
    float D = d_ggx(ndoth, roughness);
    vec3 Fr = D * F * Vis;

    vec3 Fd = mix(albedo, vec3(0.0), metallic) / M_PI * (1.0 - F);

    return Fr + Fd;
}

#endif