#version 460

layout(binding = 1, set = 0) uniform samplerCube environment_map;

layout(location = 0) in vec3 frag_pos;

layout(location = 0) out vec4 color;


void main()
{
    vec3 dir = normalize(frag_pos);

    vec3 major = vec3(0.0);
    if (dir.x > dir.y)
        if (dir.x > dir.z)
            major = vec3(1.0, 0.0, 0.0);
        else
            major = vec3(0.0, 0.0, 1.0);
    else
        if (dir.y > dir.z)
            major = vec3(0.0, 1.0, 0.0);
        else
            major = vec3(0.0, 0.0, 1.0);
    
    vec3 c = step(vec3(0.0), dir) * major;

    vec3 env_sample = texture(environment_map, dir).rgb;

    color = vec4(env_sample, 1.0);
}