#ifndef BLUR_COMMON_GLSL
#define BLUR_COMMON_GLSL

#include "math.glsl"
#include "misc.glsl"

const vec3 poisson_samples[ 8 ] =
{
    // https://www.desmos.com/calculator/abaqyvswem
    vec3( -1.00             ,  0.00             , 1.0 ),
    vec3(  0.00             ,  1.00             , 1.0 ),
    vec3(  1.00             ,  0.00             , 1.0 ),
    vec3(  0.00             , -1.00             , 1.0 ),
    vec3( -0.25 * sqrt(2.0) ,  0.25 * sqrt(2.0) , 0.5 ),
    vec3(  0.25 * sqrt(2.0) ,  0.25 * sqrt(2.0) , 0.5 ),
    vec3(  0.25 * sqrt(2.0) , -0.25 * sqrt(2.0) , 0.5 ),
    vec3( -0.25 * sqrt(2.0) , -0.25 * sqrt(2.0) , 0.5 )
};

float get_gaussian_weight(float r)
{
    return exp(-0.66 * r * r);
}

vec2 get_geometry_weight_params(float plane_dist_sensitivity, float frustum_size, vec3 X, vec3 N, float inv_accum_speed)
{
    float relaxation = mix(1.0, 0.25, inv_accum_speed);
    float a = relaxation / (plane_dist_sensitivity * frustum_size);
    float b = -dot(X, N) * a;

    return vec2(a, b);
}

float get_specular_lobe_half_angle( float linear_roughness, float percent_of_volume /* = 0.75 */ )
{
    float m = linear_roughness * linear_roughness;

    // Comparison of two methods:
    // https://www.desmos.com/calculator/4vvg1qrec7
    #if 1
        // https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf (page 72)
        // TODO: % of NDF volume - is it the trimming factor from VNDF sampling?
        return atan( m * percent_of_volume / ( 1.0 - percent_of_volume ) );
    #else
        return M_PI * m / ( 1.0 + m );
    #endif
}

float get_normal_weight_params(float inv_accum_speed, float lobe_fraction, float roughness)
{
    float angle = get_specular_lobe_half_angle(roughness, 0.75);
    angle *= mix(lobe_fraction, 1.0, inv_accum_speed);

    return 1.0 / max(angle, 0.01);
}

vec2 get_hit_distance_weight_params(float hit_dist, float inv_accum_speed)
{
    float norm = mix( 0.001, 1.0, inv_accum_speed);
    float a = 1.0 / norm;
    float b = hit_dist * a;

    return vec2(a, -b);
}

float get_roughness_weight(float roughness0, float roughness)
{
    float norm = roughness0 * roughness0 * 0.99 + 0.01;
    float w = abs(roughness0 - roughness) / norm;

    return clamp(1.0 - w, 0.0, 1.0);
}

mat3 get_kernel_basis(vec3 V, vec3 N, float roughness)
{
    mat3 tbn = create_tangent_space(N);
    vec3 T = tbn[0];
    vec3 B = tbn[1];

    float NoV = abs(dot(N, V));
    float f = get_specular_dominant_factor(NoV, roughness);
    vec3 R = reflect(-V, N);
    vec3 D = normalize(mix(N, R, f));
    float NoD = abs(dot(N, D));

    if (NoD < 0.999 && roughness != 1.0)
    {
        vec3 Dreflected = reflect(-D, N);
        T = normalize(cross(N, Dreflected));
        B = cross(Dreflected, T);

        float acos01sq = clamp(1.0 - NoV, 0.0, 1.0);
        float skew_factor = mix(1.0, roughness, sqrt(acos01sq));
        T *= skew_factor;
    }

    return mat3(T, B, N);
}

#define NUM_SAMPLES 8

#endif