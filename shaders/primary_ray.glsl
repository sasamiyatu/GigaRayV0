#ifndef PRIMARY_RAY_GLSL
#define PRIMARY_RA_GLSL

struct Ray_Payload
{
    vec2 barycentrics;
    float t;
    int instance_id;
    int prim_id;
};

#endif