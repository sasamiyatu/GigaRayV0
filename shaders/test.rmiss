#version 460
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "test.glsl"

layout(location = 0) rayPayloadInEXT Ray_Payload pay;

void main()
{
    //debugPrintfEXT("rmiss");
    pay.color = vec3(0.0, 0.0, 1.0);
    pay.prim_id = -1;
}