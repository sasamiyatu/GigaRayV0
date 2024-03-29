#ifndef SCENE_GLSL
#define SCENE_GLSL

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "test.glsl"
#include "../shared/shared.h"
#include "misc.glsl"

layout(set = 0, binding = 2, scalar) uniform camera_buffer{
    Camera_Data current;
    Camera_Data previous;
} camera_data;

#ifdef RAY_TRACING
layout(binding = 1, set = 0) uniform accelerationStructureEXT scene;
#endif
layout(binding = 3, set = 0) uniform sampler2D environment_map;
layout(binding = 4, set = 0) uniform samplerCube envmap_cube;

layout (set = 1, binding = 0) uniform sampler2D textures[];
layout (set = 1, binding = 1, scalar) readonly buffer vertex_buffer_array_t
{
    Vertex_Data verts[];
} vertex_buffer_array[];
layout (set = 1, binding = 2, scalar) readonly buffer index_buffer_array_t
{
    Index_Data indices[];
} index_buffer_array[];
layout (set = 1, binding = 3, scalar) readonly buffer material_array_t
{
    Material materials[];
} material_array;

layout(set = 1, binding = 4, scalar) readonly buffer primitive_info_t
{
    Primitive_Info primitives[];
} primitive_info_array[];

Vertex get_interpolated_vertex(int custom_instance_id, int primitive_id, vec2 barycentrics)
{
    int geom_id = int((custom_instance_id) & 0x3FFF);
    int prim_id = int((custom_instance_id >> 14) & 0x3FF);

    Primitive_Info prim_info = primitive_info_array[geom_id].primitives[prim_id];

    uint material_id = prim_info.material_index;

    uvec3 inds = index_buffer_array[geom_id].indices[primitive_id + prim_info.vertex_offset / 3].index;
    vec3 v0 = vertex_buffer_array[geom_id].verts[inds.x].pos;
    vec3 v1 = vertex_buffer_array[geom_id].verts[inds.y].pos;
    vec3 v2 = vertex_buffer_array[geom_id].verts[inds.z].pos;

    vec3 n0 = vertex_buffer_array[geom_id].verts[inds.x].normal;
    vec3 n1 = vertex_buffer_array[geom_id].verts[inds.y].normal;
    vec3 n2 = vertex_buffer_array[geom_id].verts[inds.z].normal;

    vec2 t0 = vertex_buffer_array[geom_id].verts[inds.x].texcoord;
    vec2 t1 = vertex_buffer_array[geom_id].verts[inds.y].texcoord;
    vec2 t2 = vertex_buffer_array[geom_id].verts[inds.z].texcoord;

    vec3 N = normalize((1.0 - barycentrics.x - barycentrics.y) * n0 + barycentrics.x * n1 + barycentrics.y * n2);
    vec3 P = (1.0 - barycentrics.x - barycentrics.y) * v0 + barycentrics.x * v1 + barycentrics.y * v2;
    vec2 TX = (1.0 - barycentrics.x - barycentrics.y) * t0 + barycentrics.x * t1 + barycentrics.y * t2;

    Vertex v;
    v.pos = P;
    v.normal = N;
    v.texcoord = TX;
    v.geometric_normal = normalize(cross(v1-v0, v2-v0));

    Material mat = material_array.materials[material_id];

    Material_Properties matprop;
    vec3 albedo = mat.base_color_tex != -1 ? textureLod(textures[mat.base_color_tex], v.texcoord, 0).rgb : mat.base_color_factor.rgb;
    albedo = srgb_to_linear(albedo);

    vec2 metallic_roughness = mat.metallic_roughness_tex != -1 ? textureLod(textures[mat.metallic_roughness_tex], v.texcoord, 0).bg : vec2(mat.metallic_factor, mat.roughness_factor);

    v.material.base_color = albedo;
    v.material.metallic = metallic_roughness.x;
    v.material.roughness = metallic_roughness.y;

    return v;
}

#endif