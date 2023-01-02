#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "lightmap.glsl"

layout(location = 0) rayPayloadInEXT Ray_Payload pay;
hitAttributeEXT vec2 bary;

void main()
{
    pay.barycentrics = bary;
    pay.prim_id = gl_PrimitiveID;
    pay.instance_id = gl_InstanceCustomIndexEXT;
    pay.t = gl_HitTEXT;
    pay.object_to_world = mat4(gl_ObjectToWorldEXT);
    pay.world_to_object = mat4(gl_WorldToObjectEXT);
}