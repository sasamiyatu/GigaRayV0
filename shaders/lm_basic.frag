#version 460

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 texcoord0;
layout(location = 2) in vec2 texcoord1;
layout(location = 3) in vec3 normal;
layout(location = 4) in vec3 pos;

layout(location = 0) out vec4 out_color;

layout (binding = 1, set = 0) uniform sampler2D albedo_tex;
layout (binding = 2, set = 0) uniform sampler2D checkerboard_tex;

void main()
{
    vec4 albedo = texture(albedo_tex, texcoord1);
    ivec2 pixelcoord = ivec2(textureSize(checkerboard_tex, 0) * texcoord1);
    int odd = (pixelcoord.x ^ pixelcoord.y) & 1;
    vec3 c = odd == 1 ? vec3(0.25) : vec3(0.5);
    vec4 checker = texture(checkerboard_tex, texcoord1);
    //out_color = vec4(c * in_color, 1.0);
    out_color = vec4(normal * 0.5 + 0.5, 1.0);
}
