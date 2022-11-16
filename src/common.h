#pragma once
#include "defines.h"

// Common helper functions used everywhere

/*
	Read the entire file and allocates memory for it. Returns the number
	of bytes read. 
*/
uint32_t read_entire_file(const char* filepath, uint8_t** data);