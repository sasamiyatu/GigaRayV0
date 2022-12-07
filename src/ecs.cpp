#include "ecs.h"
#include "g_math.h"

void Camera_Component::set_transform(Transform_Component* xform)
{
	origin = xform->pos;
	up = glm::vec3(0.0, 1.0, 0.0);
	forward = -math::forward_vector(xform->rotation);

	dirty = true;
}

glm::mat4 Camera_Component::get_projection_matrix(float aspect_ratio, float znear, float zfar)
{
	return glm::perspective(glm::radians(fov), aspect_ratio, znear, zfar);
}

glm::mat4 Camera_Component::get_view_matrix()
{
	return glm::lookAt(origin, origin + forward, up);
}
