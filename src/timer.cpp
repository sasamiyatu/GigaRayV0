#include "timer.h"
#include "SDL.h"

Timer::Timer()
{
	pfreq = SDL_GetPerformanceFrequency();
	inv_pfreq = double(1.0 / pfreq);
	start = SDL_GetPerformanceCounter();
	ticks = start;
}

float Timer::update()
{
	u64 new_ticks = SDL_GetPerformanceCounter();
	u64 diff = new_ticks - ticks;
	float dt = (float)((double)diff * inv_pfreq);
	ticks = new_ticks;
	return dt;
}

double Timer::get_current_time()
{
	return (double)SDL_GetPerformanceCounter() * inv_pfreq;
}
