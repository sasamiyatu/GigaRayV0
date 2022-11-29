#ifndef MATH_H
#define MATH_H
#include "random.glsl"


#define M_PI 3.14159265359
#define ONE_OVER_PI (1.0 / M_PI)
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


#endif