#ifndef MISC_GLSL
#define MISC_GLSL

// normal is geometric normal
vec3 offset_ray(vec3 p, vec3 n)
{
    const float int_scale = 256.0f;
    const float float_scale = 1.0f / 65536.0f;
    const float origin = 1.0f / 32.0f;
    ivec3 of_i = ivec3(int_scale * n);

    vec3 p_i = vec3(
        intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0.0) ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0.0) ? -of_i.y : of_i.y)),
        intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0.0) ? -of_i.z : of_i.z))
    );

    return vec3(
        abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
        abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
        abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z
    );
}

float get_view_z(float depth, mat4 proj)
{
    return proj[3][2] / depth;
}

vec3 get_view_pos(vec3 uv_depth, mat4 proj)
{
    float aspect = proj[1][1] / proj[0][0];
    float tan_half_fov_y = 1.f / proj[1][1];

    vec3 res;
    res.z = proj[3][2] / uv_depth.z;
    res.xy = res.z * tan_half_fov_y * uv_depth.xy * vec2(aspect, 1.0);
    res.z = -res.z;

    return res;
}

#endif