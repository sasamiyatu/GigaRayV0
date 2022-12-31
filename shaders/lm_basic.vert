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
// layout(binding = 0, set = 0, scalar) readonly buffer vertex_positions
// {
//     vec3 verts[];
// };

// layout (binding = 1, set = 0, scalar) readonly buffer vertex_normals
// {
//     vec3 normals[];
// };

// layout (binding = 2, set = 0, scalar) readonly buffer vertex_uvs
// {
//     vec2 uvs[];
// };

vec3 positions[3] = {
    vec3(-0.5, -0.5, 0.5),
    vec3(0.5, -0.5, 0.5),
    vec3(0.0, 0.5, 0.5)
};

layout( push_constant ) uniform constants
{
    mat4 viewproj;
} control;

layout (location = 0) out vec3 color;
layout (location = 1) out vec2 texcoord0;
layout (location = 2) out vec2 texcoord1;

void main()
{
    //color = normals[gl_VertexIndex] * 0.5 + 0.5;
    Vertex v = verts[gl_VertexIndex];
    color = v.color;
    gl_Position = control.viewproj * vec4(v.position, 1.0);
    //gl_Position = vec4(v.uv1 * 2.0 - 1.0, 0.5, 1.0);
    texcoord0 = v.uv0;
    texcoord1 = v.uv1;
    //gl_Position = control.viewproj * vec4(verts[gl_VertexIndex], 1.0);
    //texcoord = uvs[gl_VertexIndex];
    //color = vec3(1.0, 0.0, 0.0);
    //gl_Position = control.viewproj * vec4(positions[gl_VertexIndex], 1.0);
}