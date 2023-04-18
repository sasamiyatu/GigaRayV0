#pragma once
#include "r_mesh.h"
#include "../shared/shared.h"

struct Scene
{
	std::optional<Acceleration_Structure> tlas;
	struct Camera_Component* active_camera;
	Camera_Data current_frame_camera;
	Camera_Data previous_frame_camera;
	Vk_Allocated_Buffer material_buffer;
};