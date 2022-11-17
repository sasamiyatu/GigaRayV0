#version 460
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require

struct Ray_Payload
{
    vec3 color;
    vec3 barycentrics;
    int prim_id;
};

layout(location = 0) rayPayloadInEXT Ray_Payload pay;
hitAttributeEXT vec2 bary;

void main()
{
    //debugPrintfEXT("rhit");
    vec3 red = vec3(1.0, 0.0, 0.0);
    vec3 green = vec3(0.0, 1.0, 0.0);
    vec3 blue = vec3(0.0, 0.0, 1.0);
    pay.color = (1.0 - bary.x - bary.y) * red + bary.x * green + bary.y * blue;
    pay.barycentrics = vec3(1.0 - bary.x - bary.y, bary.x, bary.y);
    pay.prim_id = gl_PrimitiveID;
    //pay.color = vec3(1.0, 0.0, 0.0);
}