#pragma once
#include "defines.h"
#include "SDL.h"

struct Mouse_State;

extern u8 keys[256];
extern Mouse_State mouse_state;

struct Controller
{
	u32 entity_id;
};

struct Mouse_State
{
	int x, y;
	int xrel, yrel;
};

void handle_key_event(SDL_Event event);
void handle_mouse_event(SDL_Event event);