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

layout (set = 1, binding = 0, scalar) readonly buffer vertex_buffer_array_t
{
    Vertex verts[];
} vertex_buffer_array[];

layout (set = 1, binding = 1, scalar) readonly buffer index_buffer_array_t
{
    Index_Data indices[];
} index_buffer_array[];