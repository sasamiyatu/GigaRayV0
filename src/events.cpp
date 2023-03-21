#include "events.h"

void Event_System::register_listener(Event_Type type, event_handler* handler)
{
	Event_Listener listener{};
	listener.type = type;
	listener.handler = handler;
	listeners.push_back(listener);
}

void Event_System::fire_event(Event ev)
{
	for (const auto& l : listeners)
	{
		if (l.type == ev.type)
			if (l.handler(ev)) break;
	}
}

Event_System events;
Event_System* g_event_system = &events;