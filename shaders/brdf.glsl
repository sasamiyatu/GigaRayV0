#ifndef BRDF_H
#define BRDF_H

#include "color.glsl"
#include "material.glsl"
#include "ggx.glsl"

#define DIFFUSE_TYPE 1
#define SPECULAR_TYPE 2

#define DIELECTRIC_F0 vec3(0.04)

struct brdf_data
{
    vec3 specular_f0;
    vec3 diffuse_reflectance;

    float roughness;        // artist input
    float alpha;            // roughness used in brdf equations
    float alpha_squared;    // precalculated 

    vec3 F; // Fresnel term
    vec3 V; // View vector
    vec3 N; // Shading normal
    vec3 H; // Half vector (microfacet normal)
    vec3 L; // Light direction

    float ndotl;
    float ndotv;

    float ldoth;
    float ndoth;
    float vdoth;
};

float luminance(vec3 rgb)
{
	return dot(rgb, vec3(0.2126f, 0.7152f, 0.0722f));
}

brdf_data prepare_brdf_data(vec3 view, vec3 shading_normal, vec3 light_direction, vec3 half_vector, Material_Properties mat)
{
    brdf_data data;

    data.specular_f0 = mix(DIELECTRIC_F0, mat.base_color, mat.metallic);
    data.diffuse_reflectance = mat.base_color * (1.f - mat.metallic);

    data.roughness = mat.roughness;
#ifdef SQUARE_ROUGHNESS
    data.alpha = mat.roughness * mat.roughness;
#else
    data.alpha = mat.roughness;
#endif
    data.alpha_squared = data.alpha * data.alpha;

    data.V = view;
    data.N = shading_normal;
    data.H = half_vector;
    data.L = light_direction;

    data.ndotl = max(0.00001, dot(data.N, data.L));
    data.ndotv = max(0.00001, dot(data.N, data.V));

    data.ldoth = max(0.0, dot(data.L, data.H));
    data.ndoth = max(0.0, dot(data.N, data.H));
    data.vdoth = max(0.0, dot(data.V, data.H));

    data.F = f_schlick(data.specular_f0, vec3(1.0), data.ldoth);

    return data;
}

float GGX_D(float alphaSquared, float NdotH) {
	float b = ((alphaSquared - 1.0f) * NdotH * NdotH + 1.0f);
	return alphaSquared / (M_PI * b * b);
}

// Smith G1 term (masking function) further optimized for GGX distribution (by substituting G_a into G1_GGX)
float Smith_G1_GGX(float alpha, float NdotS, float alphaSquared, float NdotSSquared) {
	return 2.0f / (sqrt(((alphaSquared * (1.0f - NdotSSquared)) + NdotSSquared) / NdotSSquared) + 1.0f);
}

// Smith G2 term (masking-shadowing function) for GGX distribution
// Height correlated version - optimized by substituing G_Lambda for G_Lambda_GGX and dividing by (4 * NdotL * NdotV) to cancel out 
// the terms in specular BRDF denominator
// Source: "Moving Frostbite to Physically Based Rendering" by Lagarde & de Rousiers
// Note that returned value is G2 / (4 * NdotL * NdotV) and therefore includes division by specular BRDF denominator
float Smith_G2_Height_Correlated_GGX_Lagarde(float alphaSquared, float NdotL, float NdotV) {
	float a = NdotV * sqrt(alphaSquared + NdotL * (NdotL - alphaSquared * NdotL));
	float b = NdotL * sqrt(alphaSquared + NdotV * (NdotV - alphaSquared * NdotV));
	return 0.5f / (a + b + 0.0001);
}

// A fraction G2/G1 where G2 is height correlated can be expressed using only G1 terms
// Source: "Implementing a Simple Anisotropic Rough Diffuse Material with Stochastic Evaluation", Appendix A by Heitz & Dupuy
float Smith_G2_Over_G1_Height_Correlated(float alpha, float alphaSquared, float NdotL, float NdotV) {
	float G1V = Smith_G1_GGX(alpha, NdotV, alphaSquared, NdotV * NdotV);
	float G1L = Smith_G1_GGX(alpha, NdotL, alphaSquared, NdotL * NdotL);
	return G1L / (G1V + G1L - G1V * G1L);
}

// Weight for the reflection ray sampled from GGX distribution using VNDF method
float specular_sample_weight_ggx_vndf(float alpha, float alpha_squared, float ndotl, float ndotv, float hdotl, float ndoth)
{
    return Smith_G2_Over_G1_Height_Correlated(alpha, alpha_squared, ndotl, ndotv);
}

vec3 sample_specular_microfacet(vec3 V, float alpha, float alpha_squared, vec3 specular_f0, vec2 u, out vec3 brdf_weight)
{
    vec3 H;
    if (alpha == 0.0f)
    {
        H = vec3(0.0, 0.0, 1.0);
    }
    else
    {
        H = sampleGGXVNDF(V, alpha, u);
    }

    vec3 L_i = reflect(-V, H);

    float hdotl = max(0.00001, dot(H, L_i));
    const vec3 N = vec3(0.0, 0.0, 1.0);
    float ndotl = max(0.00001, dot(N, L_i));
    float ndotv = max(0.00001, dot(N, V));
    float ndoth = max(0.00001, dot(N, H));

    vec3 F = f_schlick(specular_f0, vec3(1.0), hdotl);

    brdf_weight = F * specular_sample_weight_ggx_vndf(alpha, alpha_squared, ndotl, ndotv, hdotl, ndoth);

    return L_i;
}

bool eval_indirect_combined_brdf(
    vec2 u, vec3 shading_normal, vec3 geometric_normal, 
    vec3 V_world, Material_Properties material, const int brdf_type, 
    out vec3 new_ray_dir, out vec3 brdf_weight
)
{
    //if (dot(shading_normal, V_world) <= 0.f) return false;

    vec4 q_rotation_to_z = get_rotation_to_z_axis(shading_normal);
    vec3 V = rotate_point(q_rotation_to_z, V_world); // Transform to tangent space
    const vec3 N = vec3(0.0f, 0.0f, 1.f); // Tangent space normal

    vec3 L = vec3(0.0);

    if (brdf_type == DIFFUSE_TYPE)
    {
        L = random_cosine_hemisphere(u);
        vec3 H = normalize(L + V);

        const brdf_data data = prepare_brdf_data(V, N, L, H, material);

        brdf_weight = data.diffuse_reflectance; // Lambertian, simplified

        vec3 H_spec = sampleGGXVNDF(V, data.alpha, u); // FIXME: Use a different random number

        float vdoth = max(0.0001, dot(V, H_spec));
        //brdf_weight *= (vec3(1.0) - f_schlick(data.specular_f0, vec3(1.0), vdoth));
    }
    else if (brdf_type == SPECULAR_TYPE)
    {
        const brdf_data data = prepare_brdf_data(V, N, vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, 1.0), material);
        L = sample_specular_microfacet(V, data.alpha, data.alpha_squared, data.specular_f0, u, brdf_weight);
    }

    if (luminance(brdf_weight) == 0.0f) return false;

    new_ray_dir = normalize(rotate_point(invert_rotation(q_rotation_to_z), L));

    if (dot(geometric_normal, new_ray_dir) <= 0.0) return false;

    return true;
}

vec3 eval_specular(brdf_data data)
{
    float D = GGX_D(data.alpha_squared, data.ndoth);
    float G2 = Smith_G2_Height_Correlated_GGX_Lagarde(data.alpha_squared, data.ndotl, data.ndotv);
    float Vis = v_smith_ggx_correlated(data.ndotl, data.ndotv, data.alpha);
    
   // float D = d_ggx(data.ndoth, data.alpha);
    return data.F * G2 * D * data.ndotl;
    //return vec3(G2 * D * data.ndotl);
    //return data.F * G2 * D * data.ndotl;
}

vec3 eval_diffuse(const brdf_data data) {
    // Lambertian
	return data.diffuse_reflectance * (ONE_OVER_PI * data.ndotl); 
}

vec3 eval_combined_brdf(vec3 N, vec3 L, vec3 V, Material_Properties mat)
{
    vec3 H = normalize(L + V);
    const brdf_data data = prepare_brdf_data(V, N, L, H, mat);

    vec3 specular = eval_specular(data);
    vec3 diffuse = eval_diffuse(data);
    return specular + (1.0 - data.F) * diffuse;
}

#endif