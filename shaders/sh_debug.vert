#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable
#include "test.glsl"

layout (set = 0, binding = 0, scalar) readonly buffer vertex_buffer
{
    Vertex_Data verts[];
};
layout (set = 0, binding = 1, scalar) readonly buffer index_buffer
{
    Index_Data indices[];
};

layout(set = 0, binding = 2, scalar) uniform camera_buffer{
	mat4 view;
	mat4 proj;
    uvec4 frame_index;
} camera_data;

layout( push_constant ) uniform constants
{
    uvec3 probe_counts;
    float probe_spacing;
    vec3 probe_min;
} control;

layout (location = 0) out vec3 normal;
layout (location = 1) flat out uint probe_index;
// layout (location = 1) out vec3 base_color;
// layout (location = 2) out vec2 texcoord;
// layout (location = 3) out vec3 frag_pos;
// layout (location = 4) flat out vec3 camera_pos;
// layout (location = 5) flat out float roughness;
// layout (location = 6) flat out Material mat;

void main()
{
    uint linear_idx = gl_InstanceIndex;
    uint slice_size = control.probe_counts.x * control.probe_counts.y;
    uint z = linear_idx / slice_size;
    uint linear_2d = linear_idx % slice_size;
    uint y = linear_2d / control.probe_counts.x;
    uint x = linear_2d % control.probe_counts.x;

    //debugPrintfEXT("%f %f %f\n %f\n%f %f %f", control.probe_counts.x, control.probe_counts.y, control.probe_counts.z, control.probe_spacing, control.probe_min.x, control.probe_min.y, control.probe_min.z);

    vec3 probe_pos = control.probe_min + vec3(x, y, z) * control.probe_spacing;

    probe_index = linear_idx;
    mat4 xform = camera_data.proj * camera_data.view;
    vec3 pos = verts[gl_VertexIndex].pos + probe_pos;
    normal = verts[gl_VertexIndex].normal;
    vec4 hpos = xform * vec4(pos, 1.0);
    gl_Position = hpos;
}