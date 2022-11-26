#ifndef MATH_H
#define MATH_H
#include "random.glsl"


#define M_PI 3.14159265359
// Frisvad
mat3 create_tangent_space(vec3 n)
{
    if(n.z < -0.9999999) // Handle the singularity
    {
        return mat3(
            vec3(0.0, -1.0, 0.0),
            vec3(-1.0, 0.0, 0.0),
            n
        );
    }
    
    const float a = 1.0/(1.0 + n.z );
    const float b = -n.x * n.y *a ;
    vec3 b1 = vec3(1.0 - n.x * n.x * a, b, -n.x);
    vec3 b2 = vec3(b, 1.0 - n.y * n.y * a, -n.y);

    return mat3(b1, b2, n);
}

vec3 random_cosine_hemisphere(vec3 n, inout uvec4 seed)
{
    vec2 rand = vec2(pcg4d(seed)) * ldexp(2.0, -32);
    vec3 dir = vec3(
        sqrt(rand.x) * cos(2.0 * M_PI * rand.y),
        sqrt(rand.x) * sin(2.0 * M_PI * rand.y),
        sqrt(1.0 - rand.x)
    );

    return create_tangent_space(n) * dir;
}

float pdf_cosine_hempishere(vec3 n, vec3 l)
{
    float ndotl = max(0.0, dot(n, l));
    return ndotl / M_PI;
}







#endif