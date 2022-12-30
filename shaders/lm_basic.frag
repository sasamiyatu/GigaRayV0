#version 460

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 texcoord0;
layout(location = 2) in vec2 texcoord1;

layout(location = 0) out vec4 out_color;

layout (binding = 1, set = 0) uniform sampler2D albedo_tex;
layout (binding = 2, set = 0) uniform sampler2D checkerboard_tex;

void main()
{
    vec4 albedo = texture(albedo_tex, texcoord1);
    vec4 checker = texture(checkerboard_tex, texcoord1);
    out_color = vec4(checker.rrr * in_color, 1.0);
    //out_color = vec4(albedo.rgb, 1.0);
    //out_color = vec4(in_color, 1.0);
}