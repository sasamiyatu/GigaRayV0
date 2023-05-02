#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable
#include "scene.glsl"

layout (location = 0) out vec3 normal;
layout (location = 1) out vec3 base_color;
layout (location = 2) out vec2 texcoord;
layout (location = 3) out vec3 frag_pos;
layout (location = 4) flat out vec3 camera_pos;
layout (location = 5) flat out float roughness;
layout (location = 6) out vec3 view_z;
layout (location = 7) flat out Material mat;

layout( push_constant, scalar ) uniform constants
{
    uvec3 probe_counts;
    float probe_spacing;
    vec3 probe_min;
    vec2 jitter;
    vec2 screen_size;
} control;

void main()
{
    int geom_id = int((gl_InstanceIndex) & 0x3FFF);
    int material_id = int((gl_InstanceIndex >> 14) & 0x3FF);
    mat = material_array.materials[material_id];

    Material mat = material_array.materials[material_id];
    base_color = mat.base_color_factor.rgb;
    mat4 proj = camera_data.current.proj;
    //proj[2][0] += (control.jitter.x - 0.5) * control.screen_size.x;
    //proj[2][1] += (control.jitter.y - 0.5) * control.screen_size.y;

    mat4 xform =  proj * camera_data.current.view;
    vec3 pos = vertex_buffer_array[geom_id].verts[gl_VertexIndex].pos;
    //pos *= 0.01;
    normal = vertex_buffer_array[geom_id].verts[gl_VertexIndex].normal;
    texcoord = vertex_buffer_array[geom_id].verts[gl_VertexIndex].texcoord;
    vec4 view_pos = camera_data.current.view * vec4(pos, 1.0);
    view_z = view_pos.xyz;
    vec4 hpos = xform * vec4(pos, 1.0);
    frag_pos = pos;
    mat4 inv_view = inverse(camera_data.current.view);
    camera_pos = inv_view[3].xyz;
    roughness = mat.roughness_factor;
    gl_Position = hpos;
}