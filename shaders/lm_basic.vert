#version 460
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_debug_printf : enable
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

layout(binding = 3, set = 0) readonly buffer camera_buffer
{
    mat4 viewproj;
	mat4 view;
	mat4 inverse_view;
	mat4 proj;
};

vec3 positions[3] = {
    vec3(-0.5, -0.5, 0.5),
    vec3(0.5, -0.5, 0.5),
    vec3(0.0, 0.5, 0.5)
};

layout( push_constant ) uniform constants
{
    mat4 model;
    uint output_mode;
    uint uv_space;
} control;


layout (location = 0) out vec3 color;
layout (location = 1) out vec2 texcoord0;
layout (location = 2) out vec2 texcoord1;
layout (location = 3) out vec3 normal;
layout (location = 4) out vec3 pos;

void main()
{
    //color = normals[gl_VertexIndex] * 0.5 + 0.5;
    Vertex v = verts[gl_VertexIndex];
    color = v.color;
    if (control.uv_space == 1)
        gl_Position = vec4(v.uv1 * 2.0 - 1.0, 0.5, 1.0);
    else
        gl_Position = viewproj * control.model * vec4(v.position, 1.0);
    pos = (control.model * vec4(v.position, 1.0)).xyz;
    normal = vec3(control.model * vec4(v.normal, 0.0));
    texcoord0 = v.uv0;
    texcoord1 = v.uv1;
    //gl_Position = control.viewproj * vec4(verts[gl_VertexIndex], 1.0);
    //texcoord = uvs[gl_VertexIndex];
    //color = vec3(1.0, 0.0, 0.0);
    //gl_Position = control.viewproj * vec4(positions[gl_VertexIndex], 1.0);
}