#include "game.h"
#include "input.h"

void Game_State::simulate(float dt)
{
	auto vel = ecs->get_component<Velocity_Component>(player_entity);
	auto xform = ecs->get_component<Transform_Component>(player_entity);

	glm::mat4 orientation = glm::toMat4(xform->rotation);

	float sensitivity = 0.1f;
	float yaw_delta = (float)mouse_state.xrel * sensitivity;
	float pitch_delta = (float)mouse_state.yrel * sensitivity;

	player_state.yaw += yaw_delta;
	player_state.pitch += pitch_delta;

	mouse_state.xrel = 0;
	mouse_state.yrel = 0;

	glm::mat4 yaw_rot = glm::rotate(glm::mat4(1.f), glm::radians(-player_state.yaw), glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 pitch_rot = glm::rotate(glm::mat4(1.f), glm::radians(player_state.pitch), glm::vec3(yaw_rot[0]));

	//orientation = yaw_rot * pitch_rot;
	orientation = pitch_rot * yaw_rot;

	xform->rotation = glm::toQuat(orientation);

	const float fly_speed = 150.f;
	vel->velocity = glm::vec3(0.0);
	if (keys[SDL_SCANCODE_A])
		vel->velocity -= glm::vec3(orientation[0]) * 1.f;
	if (keys[SDL_SCANCODE_D])
		vel->velocity += glm::vec3(orientation[0]) * 1.f;
	if (keys[SDL_SCANCODE_W])
		vel->velocity -= glm::vec3(orientation[2]) * 1.f;
	if (keys[SDL_SCANCODE_S])
		vel->velocity += glm::vec3(orientation[2]) * 1.f;

	xform->pos += vel->velocity * dt * fly_speed;


	auto cam = ecs->get_component<Camera_Component>(player_entity);
	if (yaw_delta != 0.f || pitch_delta != 0.f || glm::dot(vel->velocity, vel->velocity) != 0.f)
		cam->set_transform(xform);
}
