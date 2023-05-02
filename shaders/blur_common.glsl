#ifndef BLUR_COMMON_GLSL
#define BLUR_COMMON_GLSL

#include "math.glsl"

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

float get_normal_weight_params(float inv_accum_speed, float lobe_fraction)
{
    float angle = M_PI * 0.5;
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

#define NUM_SAMPLES 8

#endif