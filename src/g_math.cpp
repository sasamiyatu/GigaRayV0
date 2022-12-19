#include "g_math.h"

namespace math
{


glm::vec3 forward_vector(glm::quat q)
{
    glm::vec3 v;
    v.x = 2.0f * (q.x * q.z + q.w * q.y);
    v.y = 2.0f * (q.y * q.z - q.w * q.x);
    v.z = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);

    return v;
}

glm::mat4 make_infinite_reverse_z_proj_rh(float fovy, float aspect, float z_near)
{
    float f = 1.0f / tanf(fovy / 2.0f);
    return glm::mat4(
        f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, z_near, 0.0f);
}


}

