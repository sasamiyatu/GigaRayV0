#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

#include "primary_ray.glsl"

layout(location = 0) rayPayloadInEXT Ray_Payload pay;
hitAttributeEXT vec2 bary;

void main()
{
    pay.barycentrics = bary;
    pay.prim_id = gl_PrimitiveID;
    pay.instance_id = gl_InstanceCustomIndexEXT;
    pay.t = gl_HitTEXT;
}