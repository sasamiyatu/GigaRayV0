#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "lightmap.glsl"

layout(location = 0) rayPayloadInEXT Ray_Payload pay;

void main()
{
    pay.prim_id = -1;
}