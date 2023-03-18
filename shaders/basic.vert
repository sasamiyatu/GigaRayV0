#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#include "scene.glsl"

layout (location = 0) out vec3 normal;
layout (location = 1) out vec3 base_color;
layout (location = 2) out vec2 texcoord;
layout (location = 3) out vec3 frag_pos;
layout (location = 4) flat out vec3 camera_pos;
layout (location = 5) flat out float roughness;
layout (location = 6) flat out Material mat;

void main()
{
    int geom_id = int((gl_InstanceIndex) & 0x3FFF);
    int material_id = int((gl_InstanceIndex >> 14) & 0x3FF);
    mat = material_array.materials[material_id];

    Material mat = material_array.materials[material_id];
    base_color = mat.base_color_factor.rgb;
    mat4 xform = camera_data.proj * camera_data.view;
    vec3 pos = vertex_buffer_array[geom_id].verts[gl_VertexIndex].pos;
    normal = vertex_buffer_array[geom_id].verts[gl_VertexIndex].normal;
    texcoord = vertex_buffer_array[geom_id].verts[gl_VertexIndex].texcoord;
    vec4 hpos = xform * vec4(pos, 1.0);
    frag_pos = pos;
    mat4 inv_view = inverse(camera_data.view);
    camera_pos = inv_view[3].xyz;
    roughness = mat.roughness_factor;
    gl_Position = hpos;
}