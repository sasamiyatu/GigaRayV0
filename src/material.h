#pragma once
#include "defines.h"

struct Material
{
	i32 base_color = -1;
	i32 metallic_roughness = -1;
	i32 normal_map = -1;

	glm::vec4 base_color_factor = glm::vec4(1.0f);
	float metallic_factor = 0.0f;
	float roughness_factor = 0.5f;
};