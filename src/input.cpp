#include "input.h"

u8 keys[256];
Mouse_State mouse_state;

void handle_key_event(SDL_Event event)
{
	if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP)
		return;

	SDL_Scancode scancode = event.key.keysym.scancode;
	if (event.type == SDL_KEYDOWN)
		keys[scancode] = 1;
	else if (event.type == SDL_KEYUP)
		keys[scancode] = 0;
}

void handle_mouse_event(SDL_Event event)
{
	mouse_state.x = event.motion.x;
	mouse_state.y = event.motion.y;
	mouse_state.xrel += event.motion.xrel;
	mouse_state.yrel += event.motion.yrel;
}