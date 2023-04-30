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


// Heatmaps from https://www.shadertoy.com/view/XtGGzG

vec3 viridis_quintic( float x )
{
	x = clamp(x, 0.0, 1.0);
	vec4 x1 = vec4( 1.0, x, x * x, x * x * x ); // 1 x x2 x3
	vec4 x2 = x1 * x1.w * x; // x4 x5 x6 x7
	return vec3(
		dot( x1.xyzw, vec4( +0.280268003, -0.143510503, +2.225793877, -14.815088879 ) ) + dot( x2.xy, vec2( +25.212752309, -11.772589584 ) ),
		dot( x1.xyzw, vec4( -0.002117546, +1.617109353, -1.909305070, +2.701152864 ) ) + dot( x2.xy, vec2( -1.685288385, +0.178738871 ) ),
		dot( x1.xyzw, vec4( +0.300805501, +2.614650302, -12.019139090, +28.933559110 ) ) + dot( x2.xy, vec2( -33.491294770, +13.762053843 ) ) );
}

vec3 plasma_quintic( float x )
{
	x = clamp(x, 0.0, 1.0);
	vec4 x1 = vec4( 1.0, x, x * x, x * x * x ); // 1 x x2 x3
	vec4 x2 = x1 * x1.w * x; // x4 x5 x6 x7
	return vec3(
		dot( x1.xyzw, vec4( +0.063861086, +1.992659096, -1.023901152, -0.490832805 ) ) + dot( x2.xy, vec2( +1.308442123, -0.914547012 ) ),
		dot( x1.xyzw, vec4( +0.049718590, -0.791144343, +2.892305078, +0.811726816 ) ) + dot( x2.xy, vec2( -4.686502417, +2.717794514 ) ),
		dot( x1.xyzw, vec4( +0.513275779, +1.580255060, -5.164414457, +4.559573646 ) ) + dot( x2.xy, vec2( -1.916810682, +0.570638854 ) ) );
}

struct Bilinear
{
    vec2 origin;
    vec2 weights;
};

Bilinear get_bilinear_filter(vec2 uv /*[0, 1]*/, vec2 tex_size)
{
    Bilinear result;
    result.origin = floor(uv * tex_size - vec2(0.5, 0.5));
    result.weights = fract(uv * tex_size - vec2(0.5, 0.5));
    return result;
}

vec4 bicubic_filter(sampler2D tex, vec2 uv, vec4 render_target_params)
{
    vec2 pos = render_target_params.zw * uv; // zw: width and height
    vec2 center_pos = floor(pos - 0.5) + 0.5; // 
    vec2 f = pos - center_pos;
    vec2 f2 = f * f;
    vec2 f3 = f * f2;

    float c = 0.5;
    vec2 w0 =        -c  * f3 +  2.0 * c        * f2 - c * f;
    vec2 w1 =  (2.0 - c) * f3 - (3.0 - c)       * f2         + 1.0;
    vec2 w2 = -(2.0 - c) * f3 + (3.0 - 2.0 * c) * f2 + c * f;
    vec2 w3 =         c  * f3 -               c * f2;

    vec2 w12 = w1 + w2;
    vec2 tc12 = render_target_params.xy * (center_pos + w2 / w12);
    vec4 center_color = texture(tex, vec2(tc12.x, tc12.y));

    vec2 tc0 = render_target_params.xy * (center_pos - 1.0);
    vec2 tc3 = render_target_params.xy * (center_pos + 2.0);
    vec4 color = texture(tex, vec2(tc12.x, tc0.y)) * (w12.x * w0.y) + 
        texture(tex, vec2(tc0.x, tc12.y)) * (w0.x * w12.y) +
        center_color * (w12.x * w12.y) + 
        texture(tex, vec2(tc3.x, tc12.y)) * (w3.x * w12.y) +
        texture(tex, vec2(tc12.x, tc3.y)) * (w12.x * w3.y);

    float total_w =  (w12.x * w0.y) + (w0.x * w12.y) + (w12.x * w12.y) + (w3.x * w12.y) + (w12.x * w3.y);

    return color / total_w;
}

#endif