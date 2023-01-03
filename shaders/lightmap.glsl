struct Ray_Payload
{
    vec2 barycentrics;
    float t;
    int instance_id;
    int prim_id;
    mat4 world_to_object;
    mat4 object_to_world;
};

struct Index_Data
{
    uvec3 index;
};

struct Vertex
{
	vec3 position;
	vec3 normal;
	vec3 color;
	vec4 tangent;
	vec2 uv0;
	vec2 uv1;
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

layout (set = 1, binding = 0, scalar) readonly buffer vertex_buffer_array_t
{
    Vertex verts[];
} vertex_buffer_array[];

layout (set = 1, binding = 1, scalar) readonly buffer index_buffer_array_t
{
    Index_Data indices[];
} index_buffer_array[];

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

Vertex get_interpolated_vertex(uint instance_id, uint primitive_id, vec2 barycentrics)
{
    uvec3 indices = index_buffer_array[instance_id].indices[primitive_id].index;
    Vertex v0 = vertex_buffer_array[instance_id].verts[indices.x];
    Vertex v1 = vertex_buffer_array[instance_id].verts[indices.y];
    Vertex v2 = vertex_buffer_array[instance_id].verts[indices.z];

    float u = barycentrics.x;
    float v = barycentrics.y;
    float w = 1.0 - barycentrics.x - barycentrics.y;

    Vertex vert;
	vert.position = v0.position * w + v1.position * u + v2.position * v;
	vert.normal = v0.normal * w + v1.normal * u + v2.normal * v;
	vert.color = v0.color * w + v1.color * u + v2.color * v;
	vert.tangent = v0.tangent * w + v1.tangent * u + v2.tangent * v;
	vert.uv0 = v0.uv0 * w + v1.uv0 * u + v2.uv0 * v;
	vert.uv1 = v0.uv1 * w + v1.uv1 * u + v2.uv1 * v;

    return vert;
}

Vertex get_interpolated_vertex2(uint instance_id, uint primitive_id, vec3 barycentrics)
{
    uvec3 indices = index_buffer_array[instance_id].indices[primitive_id].index;
    Vertex v0 = vertex_buffer_array[instance_id].verts[indices.x];
    Vertex v1 = vertex_buffer_array[instance_id].verts[indices.y];
    Vertex v2 = vertex_buffer_array[instance_id].verts[indices.z];

    Vertex vert;
	vert.position = v0.position * barycentrics.x + v1.position * barycentrics.y + v2.position * barycentrics.z;
	vert.normal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
	vert.color = v0.color * barycentrics.x + v1.color * barycentrics.y + v2.color * barycentrics.z;
	vert.tangent = v0.tangent * barycentrics.x + v1.tangent * barycentrics.y + v2.tangent * barycentrics.z;
	vert.uv0 = v0.uv0 * barycentrics.x + v1.uv0 * barycentrics.y + v2.uv0 * barycentrics.z;
	vert.uv1 = v0.uv1 * barycentrics.x + v1.uv1 * barycentrics.y + v2.uv1 * barycentrics.z;

    return vert;
}