#version 460

layout(binding = 0, set = 0) readonly buffer vertex_positions
{
    vec3 verts[];
};

layout (binding = 1, set = 0) readonly buffer vertex_normals
{
    vec3 normals[];
};

layout (binding = 2, set = 0) readonly buffer vertex_uvs
{
    vec2 uvs[];
};

layout (location = 0) out vec3 color;

void main()
{
    color = normals[gl_VertexIndex] * 0.5 + 0.5;
    gl_Position = vec4(verts[gl_VertexIndex], 1.0);
}