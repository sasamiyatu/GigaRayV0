#version 460

layout(set = 0, binding = 0) uniform camera_buffer{
	mat4 view;
	mat4 proj;
    uvec4 frame_index;
} camera_data;

vec3 vertices[] =
{
    // X axis
    vec3(1.0, 1.0, -1.0),
    vec3(1.0, -1.0, -1.0),
    vec3(1.0, -1.0, 1.0),
    vec3(1.0, 1.0, 1.0),
    
    // -X axis
    vec3(-1.0, 1.0, 1.0),
    vec3(-1.0, -1.0, 1.0),
    vec3(-1.0, -1.0, -1.0),
    vec3(-1.0, 1.0, -1.0),

    // Y axis
    vec3(-1.0, 1.0, 1.0),
    vec3(-1.0, 1.0, -1.0),
    vec3(1.0, 1.0, -1.0),
    vec3(1.0, 1.0, 1.0),

    // -Y axis
    vec3(1.0, -1.0, 1.0),
    vec3(1.0, -1.0, -1.0),
    vec3(-1.0, -1.0, -1.0),
    vec3(-1.0, -1.0, 1.0),

    // Z axis
    vec3(1.0, 1.0, 1.0),
    vec3(1.0, -1.0, 1.0),
    vec3(-1.0, -1.0, 1.0),
    vec3(-1.0, 1.0, 1.0),

    // -Z axis
    vec3(1.0, -1.0, -1.0),
    vec3(1.0, 1.0, -1.0),
    vec3(-1.0, 1.0, -1.0),
    vec3(-1.0, -1.0, -1.0),
};

uint indices[] = 
{
    0, 1, 2, 0, 2, 3,
    4, 5, 6, 4, 6, 7,
    8, 9, 10, 8, 10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23
};

layout(location = 0) out vec3 frag_pos;

void main()
{
    mat4 view_without_rotation = mat4(mat3(camera_data.view));
    mat4 viewproj = camera_data.proj * view_without_rotation;

    vec3 pos = vertices[indices[gl_VertexIndex]];

    frag_pos = pos;

    gl_Position = viewproj * vec4(pos, 1.0);
}