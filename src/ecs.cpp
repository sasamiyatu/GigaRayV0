#include "ecs.h"

void Camera_Component::set_transform(Transform_Component* xform)
{
	glm::mat3 rot = glm::toMat3(xform->rotation);
	glm::mat4 camera_mat = glm::mat4(rot);
	camera_mat[3] = glm::vec4(xform->pos, 1.f);
	view = glm::inverse(camera_mat);

	dirty = true;
}
