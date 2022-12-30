#version 460

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec4 out_color;

layout (binding = 3, set = 0) uniform sampler2D albedo_tex;

void main()
{
    vec4 albedo = texture(albedo_tex, texcoord);
    out_color = vec4(albedo.rgb, 1.0);
}