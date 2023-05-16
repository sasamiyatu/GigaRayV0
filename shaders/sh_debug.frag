#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "sh.glsl"
#include "misc.glsl"

layout (location = 0) in vec3 normal;
layout (location = 1) flat in uint probe_index;

layout (location = 0) out vec4 out_color;

layout(set = 0, binding = 3, scalar) buffer SH_probe_buffer
{
    SH_2 SH_probes[];
};

void main()
{
    SH_2 probe = SH_probes[probe_index];
    vec3 N = normalize(normal);
    vec3 evaluated_sh = eval_sh(probe, N);

    evaluated_sh = linear_to_srgb(evaluated_sh);
    out_color = vec4(evaluated_sh, 1.0);

    //if (probe_index > 20)
    //    out_color = vec4(1.0, 0.0, 1.0, 1.0);

}