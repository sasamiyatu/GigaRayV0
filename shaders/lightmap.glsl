struct Ray_Payload
{
    vec2 barycentrics;
    float t;
    int instance_id;
    int prim_id;
    mat4 world_to_object;
    mat4 object_to_world;
};

