#version 460

layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 pos;

layout(location = 0) out vec4 out_normal;
layout(location = 1) out vec4 out_pos;

void main()
{
    out_normal = vec4(normalize(normal), 1.0);
    out_pos = vec4(pos, 1.0);
}
