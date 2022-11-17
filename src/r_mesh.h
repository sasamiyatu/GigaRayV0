#pragma once
#include "ecs.h"
#include "renderer.h"

struct Rt_Scene
{
	Vk_Acceleration_Structure tlas;

	void initialize_renderables(ECS* ecs);
	void build_top_level_acceleration_structure(ECS* ecs);
};

Mesh* get_mesh(ECS* ecs, uint32_t entity_id);