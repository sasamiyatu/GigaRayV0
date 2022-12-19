#version 460

layout(location = 0) in vec3 in_normal;

layout(location = 0) out vec4 color;
void main()
{
    vec3 normal = normalize(in_normal);
    color = vec4(normal * 0.5 + 0.5, 1.0);
}