#pragma once
#include "defines.h"

namespace math
{


glm::vec3 forward_vector(glm::quat q);

glm::mat4 make_infinite_reverse_z_proj_rh(float fovy, float aspect, float z_near);

}