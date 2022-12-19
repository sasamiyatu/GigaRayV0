#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#include "scene.glsl"

layout(set = 0, binding = 0) uniform sampler2D a;
layout(set = 0, binding = 1) uniform sampler2D b;

vec3 verts[] = {
    vec3(0.0, 0.5, 0.0),
    vec3(-0.5, -0.5, 0.0),
    vec3(0.5, -0.5, 0.0)
};

layout (location = 0) out vec3 normal;

void main()
{
    int geom_id = int((gl_InstanceIndex) & 0x3FFF);
    int material_id = int((gl_InstanceIndex >> 14) & 0x3FF);
    
    mat4 xform = camera_data.proj * camera_data.view;
    vec3 pos = vertex_buffer_array[geom_id].verts[gl_VertexIndex].pos;
    normal = vertex_buffer_array[geom_id].verts[gl_VertexIndex].normal;
    vec4 hpos = xform * vec4(pos, 1.0);
    gl_Position = hpos;
}