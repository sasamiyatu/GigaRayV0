#include "g_math.h"

namespace math
{
void pcg32_srandom_r(pcg32_random_t* rng, uint64_t initstate, uint64_t initseq)
{
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    pcg32_random_r(rng);
    rng->state += initstate;
    pcg32_random_r(rng);
}
uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

glm::vec3 forward_vector(glm::quat q)
{
    glm::vec3 v;
    v.x = 2.0f * (q.x * q.z + q.w * q.y);
    v.y = 2.0f * (q.y * q.z - q.w * q.x);
    v.z = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);

    return v;
}

glm::mat4 make_infinite_reverse_z_proj_rh(float fovy, float aspect, float z_near)
{
    float f = 1.0f / tanf(fovy / 2.0f);
    return glm::mat4(
        f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, z_near, 0.0f);
}

glm::vec3 random_vector(u64 seed)
{
    pcg32_random_t rng{};
    pcg32_srandom_r(&rng, seed, 1);
    glm::uvec3 rand{};
    for (int i = 0; i < 3; ++i) rand[i] = pcg32_random_r(&rng);
    return glm::vec3(rand) * ldexpf(1.0f, -32);
}


}

