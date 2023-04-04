#pragma once
#include "r_mesh.h"

struct GPU_Camera_Data
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::uvec4 frame_index;
};

struct Scene
{
	std::optional<Acceleration_Structure> tlas;
	struct Camera_Component* active_camera;
	GPU_Camera_Data current_frame_camera;
	Vk_Allocated_Buffer material_buffer;
};