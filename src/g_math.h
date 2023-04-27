#pragma once
#include "defines.h"

namespace math
{
constexpr float PI = 3.14159265359f;
constexpr float PI_OVER_2 = 1.57079632679f;

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;

void pcg32_srandom_r(pcg32_random_t* rng, uint64_t initstate, uint64_t initseq);
uint32_t pcg32_random_r(pcg32_random_t* rng);

glm::vec3 forward_vector(glm::quat q);

glm::mat4 make_infinite_reverse_z_proj_rh(float fovy, float aspect, float z_near);

glm::vec3 random_vector(u64 seed);

glm::vec3 polar_to_unit_vec(float azimuth, float zenith);

}