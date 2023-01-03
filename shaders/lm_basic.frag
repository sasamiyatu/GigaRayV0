#version 460

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 texcoord0;
layout(location = 2) in vec2 texcoord1;
layout(location = 3) in vec3 normal;
layout(location = 4) in vec3 pos;
layout(location = 5) in flat uint material_index;

layout(location = 0) out vec4 out_color;

layout (binding = 1, set = 0) uniform sampler2D lightmap_bilinear_tex;
layout (binding = 2, set = 0) uniform sampler2D lightmap_tex;

layout( push_constant ) uniform constants
{
    mat4 model;
    uint output_mode;
    bool uv_space;
} control;

layout(binding = 3, set = 0) readonly buffer camera_buffer
{
    mat4 viewproj;
	mat4 view;
	mat4 inverse_view;
	mat4 proj;
};

struct Material
{
	int base_color_tex;
	int metallic_roughness_tex;
	int normal_map_tex;
	int emissive_tex;

	vec4 base_color_factor;
	float roughness_factor;
	float metallic_factor;

	vec3 emissive_factor;
};

layout(binding = 4, set = 0, scalar) readonly buffer material_buffer_t
{
    Material materials[];
} material_buffer;

void main()
{
    Material mat = material_buffer.materials[material_index];
    vec4 lm_smooth = texture(lightmap_bilinear_tex, texcoord1);
    ivec2 pixelcoord = ivec2(textureSize(lightmap_tex, 0) * texcoord1);
    int odd = (pixelcoord.x ^ pixelcoord.y) & 1;
    vec3 c = odd == 1 ? vec3(0.25) : vec3(0.5);
    vec4 lm = texture(lightmap_tex, texcoord1);
    vec3 camera_pos = inverse_view[3].xyz;
    vec3 multiplied = lm_smooth.rgb * mat.base_color_factor.rgb;
    vec3 n = normalize(normal);
    switch (control.output_mode)
    {
    case 0:
        out_color = vec4(c * in_color, 1.0);
        break;
    case 1:
        out_color = vec4(lm.rgb, 1.0);
        break;
    case 2:
        out_color = vec4(n * 0.5 + 0.5, 1.0);
        break;
    case 3:
        out_color = vec4(texcoord1, 0.0, 1.0);
        break;
    case 4:
        out_color = vec4(mat.base_color_factor.rgb, 1.0);
        break;
    case 5:
        out_color = vec4(multiplied, 1.0);
    }
}
