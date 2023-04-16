#version 460
#extension GL_GOOGLE_include_directive : enable

#include "scene.glsl"

layout (location = 0) out vec3 frag_pos;

void main()
{
    int geom_id = int((gl_InstanceIndex) & 0x3FFF);
    int material_id = int((gl_InstanceIndex >> 14) & 0x3FF);

    mat4 xform = camera_data.current.proj * camera_data.current.view;
    vec3 pos = vertex_buffer_array[geom_id].verts[gl_VertexIndex].pos;
    frag_pos = pos;
    vec4 hpos = xform * vec4(pos, 1.0);
    
    gl_Position = hpos;
}