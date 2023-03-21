#pragma once
#include "defines.h"

#define LOG_DEBUG(...) \
	log_debug(__VA_ARGS__);

void log_debug(const char* fmt, ...);