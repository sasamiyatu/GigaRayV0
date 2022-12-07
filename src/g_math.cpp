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


}

