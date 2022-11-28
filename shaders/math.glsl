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







#endif