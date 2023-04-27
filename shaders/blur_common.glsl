#ifndef BLUR_COMMON_GLSL
#define BLUR_COMMON_GLSL

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

#define NUM_SAMPLES 8

#endif