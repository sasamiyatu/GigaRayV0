#pragma once
#include "defines.h"

// Common helper functions used everywhere

/*
	Read the entire file and allocates memory for it. Returns the number
	of bytes read. 
*/
uint32_t read_entire_file(const char* filepath, uint8_t** data);

// Function to count the total number of set bits in `n`
int count_set_bits(int n);
