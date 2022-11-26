#pragma once
#include "defines.h"
struct Timer
{
	u64 pfreq;
	double inv_pfreq;
	u64 ticks;
	u64 start;


	Timer();
	float update();
};