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

vec3 linear_to_YCoCg(vec3 rgb) 
{
#if 0
    float Y = dot(rgb, vec3(0.25, 0.5, 0.25));
    float Co = dot(rgb, vec3(0.5, 0.00, -0.5));
    float Cg = dot(rgb, vec3(-0.25, 0.5, -0.25));
#else
    float Co = rgb.r - rgb.b;
    float t = rgb.b + Co * 0.5;
    float Cg = rgb.g - t;
    float Y = t + Cg * 0.5;
#endif
    return vec3(Y, Co, Cg);
}

vec3 YCoCg_to_linear(vec3 ycocg)
{
#if 0
    float t = ycocg.x - ycocg.z;

    vec3 r;
    r.y = ycocg.x + ycocg.z;
    r.x = t + ycocg.y;
    r.z = t - ycocg.y;
    return max(vec3(0.0), r);
#else
    float t = ycocg.x - ycocg.z * 0.5;
    float g = ycocg.z + t;
    float b = t - ycocg.y * 0.5;
    float r = b + ycocg.y;
    return max(vec3(0.0), vec3(r, g, b));
#endif
}

vec4 clamp_negative_to_zero(vec4 color, bool ycocg_color_space)
{
    if (ycocg_color_space)
    {
        color.rgb = YCoCg_to_linear(color.rgb);
        color = max(vec4(0), color);
        color.rgb = linear_to_YCoCg(color.rgb);
        return color;        
    }
    else
        return max(vec4(0), color);
}


vec3 linear_to_srgb(vec3 color)
{
    const vec4 consts = vec4(1.055, 0.41666, -0.055, 12.92);
    color = clamp(color, 0.0, 1.0);

    return mix(consts.x * pow(color, consts.yyy) + consts.zzz, consts.w * color, step(color, vec3(0.0031308)));
}

vec3 srgb_to_linear(vec3 color)
{
    const vec4 consts = vec4(1.0 / 12.92, 1.0 / 1.055, 0.055 / 1.055, 2.4);
    color = clamp(color, 0.0, 1.0);

    return mix(color * consts.x, pow(color * consts.y + consts.zzz, consts.www), step(vec3(0.04045), color));
}

// "Ray Tracing Gems", Chapter 32, Equation 4 - the approximation assumes GGX VNDF and Schlick's approximation
vec3 environment_term_rtg(vec3 f0, float NoV, float linear_roughness)
{
    float m = linear_roughness * linear_roughness;

    vec4 X;
    X.x = 1.0;
    X.y = NoV;
    X.z = NoV * NoV;
    X.w = NoV * X.z;

    vec4 Y;
    Y.x = 1.0;
    Y.y = m;
    Y.z = m * m;
    Y.w = m * Y.z;

    mat2 M1 = transpose(mat2( 0.99044, -1.28514, 1.29678, -0.755907 ));
    mat3 M2 = transpose(mat3( 1.0, 2.92338, 59.4188, 20.3225, -27.0302, 222.592, 121.563, 626.13, 316.627 ));

    mat2 M3 = transpose(mat2( 0.0365463, 3.32707, 9.0632, -9.04756 ));
    mat3 M4 = transpose(mat3( 1.0, 3.59685, -1.36772, 9.04401, -16.3174, 9.22949, 5.56589, 19.7886, -20.2123 ));

    float bias = dot( M1 * X.xy, Y.xy ) / ( dot( M2 * X.xyw, Y.xyw ) );
    float scale = dot( M3 * X.xy, Y.xy ) / ( dot( M4 * X.xzw, Y.xyw ) );

    return clamp( f0 * scale + bias, vec3(0.0), vec3(1.0) );
}

float get_hit_distance_normalization(vec4 hit_distance_params, float view_z, float roughness)
{
    return (hit_distance_params.x + hit_distance_params.y * abs(view_z)) * mix(1.0, hit_distance_params.z, exp2(hit_distance_params.w * roughness * roughness));
}

#endif