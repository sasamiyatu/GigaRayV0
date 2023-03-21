#version 460

layout(binding = 1, set = 0) uniform samplerCube environment_map;

layout(location = 0) in vec3 frag_pos;

layout(location = 0) out vec4 color;


void main()
{
    vec3 dir = normalize(frag_pos);

    vec3 env_sample = texture(environment_map, dir).rgb;
    //env_sample = pow(env_sample, vec3(2.2));
    color = vec4(env_sample, 1.0);
}