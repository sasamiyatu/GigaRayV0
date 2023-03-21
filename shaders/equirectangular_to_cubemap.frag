#version 460

layout (location = 0) out vec4 color;

layout (binding = 0, set = 0) uniform sampler2D environment_map;

layout( push_constant ) uniform constants
{
    uvec2 size;
    uint face_index;
} control;

mat3 get_transform_for_face(uint face_index)
{
    switch (face_index)
    {
        case 0:
            return mat3(
                0.0, 0.0, -1.0,
                0.0, -1.0, 0.0,
                -1.0, 0.0, 0.0
            );
        case 1:
            return mat3(
                0.0, 0.0, 1.0,
                0.0, -1.0, 0.0,
                1.0, 0.0, 0.0
            );
        case 2:
            return mat3(
                1.0, 0.0, 0.0,
                0.0, 0.0, 1.0,
                0.0, -1.0, 0.0
            );
        case 3:
            return mat3(
                1.0, 0.0, 0.0,
                0.0, 0.0, -1.0,
                0.0, 1.0, 0.0
            );
        case 4:
            return mat3(
                1.0, 0.0, 0.0,
                0.0, -1.0, 0.0,
                0.0, 0.0, -1.0
            );
        case 5:
            return mat3(
                -1.0, 0.0, 0.0,
                0.0, -1.0, 0.0,
                0.0, 0.0, 1.0
            );
    }
}

const vec2 inv_atan = vec2(0.1591, 0.3183);

vec2 sample_equirectangular_map(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= inv_atan;
    uv += 0.5;
    return uv;
}

layout(location = 0) in vec3 frag_pos;

void main()
{
    //vec2 uv = gl_FragCoord.xy / vec2(control.size);
    //vec3 dir = normalize(vec3(uv * 2.0f - 1.0f, -1.0f));
    //dir = get_transform_for_face(control.face_index) * dir; 
    vec3 dir = normalize(frag_pos);

    vec2 envmap_uv = sample_equirectangular_map(dir);
    vec3 envmap_sample = texture(environment_map, envmap_uv).rgb;

    color = vec4(envmap_sample, 1.0);
    //color = vec4(1.0, 0.0, 1.0, 1.0);
}