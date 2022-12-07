#ifndef TEST_H
#define TEST_H

#include "material.glsl"

struct Ray_Payload
{
    vec2 barycentrics;
    float t;
    int instance_id;
    int prim_id;
};

struct Miss_Payload
{
    int prim_id;
};

struct Vertex_Data
{
    vec3 pos;
    vec3 normal;
    vec2 texcoord;
    vec4 tangent;
};

struct Vertex
{
    vec3 pos;
    vec3 normal; 
    vec2 texcoord;
    vec4 tangent;
    vec3 geometric_normal;

    Material_Properties material;
};

struct Index_Data
{
    uvec3 index;
};

#endif