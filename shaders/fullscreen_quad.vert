#version 460

vec2 quad[] = {
    vec2(-1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0)
};

int indices[] = {0, 1, 2, 1, 3, 2};

void main()
{
    vec3 pos = vec3(quad[indices[gl_VertexIndex]], 0.5);
    gl_Position = vec4(pos, 1.0);
}