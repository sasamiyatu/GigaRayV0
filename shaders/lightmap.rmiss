#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "lightmap.glsl"

layout(location = 0) rayPayloadInEXT Ray_Payload pay;

void main()
{
    pay.prim_id = -1;
    pay.instance_id = -1;
}