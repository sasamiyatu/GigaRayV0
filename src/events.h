#pragma once
#include "SDL.h"
#include "defines.h"

constexpr u32 MAX_ARGS = 8;

enum Event_Type
{
	KEY_PRESS = 0,
};

struct Key_Event
{
	SDL_Scancode scancode;
	u32 type;
	u8 repeat;
};

struct Event
{
	Event_Type type;

	union
	{
		Key_Event key_event;
	} event;
};

typedef bool event_handler(Event& e);

struct Event_Listener
{
	Event_Type type;
	event_handler* handler;
};

struct Event_System
{
	std::vector<Event_Listener> listeners;

	void register_listener(Event_Type type, event_handler* handler);
	void fire_event(Event ev);
};

extern Event_System* g_event_system;