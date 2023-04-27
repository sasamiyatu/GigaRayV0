#pragma once
#include "ecs.h"

struct Player_State
{
	float yaw = 0.f, pitch = 0.f, roll = 0.f;
};


struct Game_State
{
	ECS* ecs;
	u32 player_entity;
	Player_State player_state;
	float fly_speed = 1.50f;

	void simulate(float dt);
	void handle_mouse_scroll(int delta);
};