#include "misc.h"

// Adapted from: https://gist.github.com/rygorous/2156668 (public domain)
float half_to_float(uint16_t half)
{
    union FP32
    {
        uint32_t u;
        float f;
        struct
        {
            uint32_t Mantissa : 23;
            uint32_t Exponent : 8;
            uint32_t Sign : 1;
        };
    };

    static const FP32 magic = { 113 << 23 };
    static const uint32_t shifted_exp = 0x7c00 << 13; // exponent mask after shift
    FP32 o;

    o.u = (half & 0x7fff) << 13;        // exponent/mantissa bits
    uint32_t exp = shifted_exp & o.u;   // just the exponent
    o.u += (127 - 15) << 23;            // exponent adjust

    // handle exponent special cases
    if (exp == shifted_exp) // Inf/NaN?
        o.u += (128 - 16) << 23;        // extra exp adjust
    else if (exp == 0) // Zero/Denormal?
    {
        o.u += 1 << 23;                 // extra exp adjust
        o.f -= magic.f;                 // renormalize
    }

    o.u |= (half & 0x8000) << 16;       // sign bit
    return o.f;
}