#version 460
#extension GL_EXT_scalar_block_layout : require

struct Vertex
{
	vec3 position;
	vec3 normal;
	vec3 color;
	vec4 tangent;
	vec2 uv0;
	vec2 uv1;
};

layout(binding = 0, set = 0, scalar) readonly buffer vertex_buffer
{
    Vertex verts[];
};

layout( push_constant ) uniform constants
{
    mat4 model;
} control;

layout (location = 0) out vec3 normal;
layout (location = 1) out vec3 pos;

void main()
{
    Vertex v = verts[gl_VertexIndex];
    pos = (control.model * vec4(v.position, 1.0)).xyz;
    normal = v.normal;
    gl_Position = vec4(v.uv1 * 2.0 - 1.0, 0.5, 1.0);
}