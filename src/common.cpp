#include "common.h"

uint32_t read_entire_file(const char* filepath, uint8_t** data)
{
	HANDLE h = CreateFileA(filepath, FILE_GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	DWORD file_size = GetFileSize(h, 0);

	uint8_t* buffer = (uint8_t*)malloc(file_size);
	DWORD bytes_read;
	bool success = ReadFile(h, buffer, file_size, &bytes_read, 0);
	assert(success);
	assert(bytes_read == (uint32_t)file_size);

	*data = buffer;
	CloseHandle(h);
	return (uint32_t)bytes_read;
}

int count_set_bits(int n)
{
    // `count` stores the total bits set in `n`
    int count = 0;

    while (n)
    {
        n = n & (n - 1);    // clear the least significant bit set
        count++;
    }

    return count;
}