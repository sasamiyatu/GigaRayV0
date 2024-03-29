#ifndef MATH_H
#define MATH_H
#include "random.glsl"

#ifndef M_PI
#define M_PI 3.14159265359
#endif
#ifndef ONE_OVER_PI
#define ONE_OVER_PI (1.0 / M_PI)
#endif

struct Ray
{
    vec3 ro;
    vec3 rd;
};
// Frisvad
mat3 create_tangent_space(vec3 n)
{
    if(n.z < -0.9999999) // Handle the singularity
    {
        return mat3(
            vec3(0.0, -1.0, 0.0),
            vec3(-1.0, 0.0, 0.0),
            n
        );
    }
    
    const float a = 1.0/(1.0 + n.z );
    const float b = -n.x * n.y *a ;
    vec3 b1 = vec3(1.0 - n.x * n.x * a, b, -n.x);
    vec3 b2 = vec3(b, 1.0 - n.y * n.y * a, -n.y);

    return mat3(b1, b2, n);
}

float saturate(float v) { return clamp(v, 0.0, 1.0); }

// Quaternion stuff

// Calculates rotation quaternion from input vector to the vector (0, 0, 1)
// Input vector must be normalized!
vec4 get_rotation_to_z_axis(vec3 v) {

	// Handle special case when input is exact or near opposite of (0, 0, 1)
	if (v.z < -0.99999f) return vec4(1.0f, 0.0f, 0.0f, 0.0f);

	return normalize(vec4(v.y, -v.x, 0.0f, 1.0f + v.z));
}

// Optimized point rotation using quaternion
// Source: https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
vec3 rotate_point(vec4 q, vec3 v) {
	const vec3 q_axis = vec3(q);
	return 2.0f * dot(q_axis, v) * q_axis + (q.w * q.w - dot(q_axis, q_axis)) * v + 2.0f * q.w * cross(q_axis, v);
}

// Returns the quaternion with inverted rotation
vec4 invert_rotation(vec4 q)
{
	return vec4(-q.x, -q.y, -q.z, q.w);
}

#ifdef RAY_TRACING
Ray get_camera_ray(mat4 view, mat4 proj, ivec2 pixel, ivec2 size)
{
    const vec2 resolution = vec2(size);

    vec2 uv = (((pixel + 0.5f) / resolution) * 2.f - 1.f);

    mat4 inv_view = inverse(view);
    vec3 ro = inv_view[3].xyz;
    float aspect = proj[1][1] / proj[0][0];
    float tan_half_fov_y = 1.f / proj[1][1];
    vec3 rd = normalize(
        (uv.x * inv_view[0].xyz * tan_half_fov_y * aspect) - 
        (uv.y * inv_view[1].xyz * tan_half_fov_y) -
        inv_view[2].xyz);

    Ray r;
    r.ro = ro;
    r.rd = rd;

    return r;
}
#endif

vec3 equirectangular_to_vec3(vec2 uv)
{
    float phi = uv.x * 2.0 * M_PI;
    float theta = uv.y * M_PI;

    vec3 dir;
    dir.z = -cos(phi)*sin(theta);
    dir.y = -cos(theta);
    dir.x = sin(phi)*sin(theta);

    return dir;
}

vec2 equirectangular_to_uv(vec3 dir)
{
    float theta = acos(-dir.y);
    float phi = atan(-dir.x, dir.z) + M_PI;
    vec2 uv = vec2(phi / (2.0 * M_PI), theta / M_PI);
    return uv;
}

// Oct packing
vec2 encode_unit_vector(vec3 v, bool signed)
{
    v /= dot(abs(v), vec3(1.0));

    vec2 oct_wrap = (1.0 - abs(v.yx)) * (step(0.0, v.xy) * 2.0 - 1.0);
    v.xy = v.z >= 0.0 ? v.xy : oct_wrap;

    return signed ? v.xy : v.xy * 0.5 + 0.5;
}

vec3 decode_unit_vector(vec2 p, bool signed, bool should_normalize)
{
    p = signed ? p : (p * 2.0 - 1.0);

    // https://twitter.com/Stubbesaurus/status/937994790553227264
    vec3 n = vec3(p.xy, 1.0 - abs(p.x) - abs(p.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.xy -= t * (step(0.0, n.xy) * 2.0 - 1.0);

    return should_normalize ? normalize(n) : n;
}

float pow5(float x)
{
    float clamped = clamp(x, 0.0, 1.0);
    return pow(1.0 - clamped, 5.0);
}

vec3 get_specular_dominant_direction(vec3 N, vec3 V, float linear_roughness)
{
    float f = (1.0 - linear_roughness) * (sqrt(1.0 - linear_roughness) + linear_roughness);
    vec3 R = reflect(-V, N);
    vec3 dir = mix(N, R, f);

    return normalize(dir);
}


float get_specular_dominant_factor(float NoV , float roughness)
{
    float a = 0.298475 * log (39.4115 - 39.0029 * roughness);
    float f = pow(clamp (1.0 - NoV, 0.0, 1.0), 10.8649) * (1.0 - a) + a;
    return clamp(f, 0.0, 1.0);
}

#endif