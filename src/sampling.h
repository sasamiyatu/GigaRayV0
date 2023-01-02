#pragma once
#include "defines.h"
#include <algorithm>

constexpr float one_minus_epsilon = 0x1.fffffep-1;

inline u32 reverse_bits_32(u32 n) {
    n = (n << 16) | (n >> 16);
    n = ((n & 0x00ff00ff) << 8) | ((n & 0xff00ff00) >> 8);
    n = ((n & 0x0f0f0f0f) << 4) | ((n & 0xf0f0f0f0) >> 4);
    n = ((n & 0x33333333) << 2) | ((n & 0xcccccccc) >> 2);
    n = ((n & 0x55555555) << 1) | ((n & 0xaaaaaaaa) >> 1);
    return n;
}

inline u64 ReverseBits64(u64 n) {
    u64 n0 = reverse_bits_32((u32)n);
    u64 n1 = reverse_bits_32((u32)(n >> 32));
    return (n0 << 32) | n1;
}

inline float van_der_corput(u32 i)
{
    u32 reversed = reverse_bits_32(i);
    return (float)reversed * ldexpf(1.0, -32);
}

template<int base>
inline constexpr float radical_inverse(u64 i)
{
    constexpr float inv_base = 1.0f / (float)base;
    u64 reversed_digits = 0;
    float inv_base_n = 1.0f;
    while (i)
    {
        u64 next = i / base;
        u64 digit = i - next * base;
        reversed_digits = reversed_digits * base + digit;
        inv_base_n *= inv_base;
        i = next;
    }
    return std::min(reversed_digits * inv_base_n, one_minus_epsilon);
}

template<int base1, int base2>
inline constexpr glm::vec2 radical_inverse_vec2(u64 i)
{
    return glm::vec2(radical_inverse<base1>(i), radical_inverse<base2>(i));
}