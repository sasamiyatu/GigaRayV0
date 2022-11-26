
struct Ray_Payload
{
    vec2 barycentrics;
    float t;
    int instance_id;
    int prim_id;
};

struct Vertex_Data
{
    vec3 pos;
    vec3 normal;
    vec2 texcoord;
};

struct Vertex
{
    vec3 pos;
    vec3 normal; 
    vec2 texcoord;
};

struct Index_Data
{
    uvec3 index;
};