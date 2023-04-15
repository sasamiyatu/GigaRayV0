#ifndef SH_GLSL
#define SH_GLSL

struct SH_2
{
    vec3 coefs[9];
};

vec3 eval_sh(SH_2 sh, vec3 n)
{
    vec3 sh_result[9];
    sh_result[0] = 0.282095f * sh.coefs[0];
    sh_result[1] = -0.488603f * n.y * sh.coefs[1];
    sh_result[2] = 0.488603f * n.z * sh.coefs[2];
    sh_result[3] = -0.488603f * n.x * sh.coefs[3];
    sh_result[4] = 1.092548f * n.x * n.y * sh.coefs[4];
    sh_result[5] = -1.092548f * n.y * n.z * sh.coefs[5];
    sh_result[6] = 0.315392f * (3.0f * n.z * n.z - 1.0f) * sh.coefs[6];
    sh_result[7] = -1.092548f * n.x * n.z * sh.coefs[7];
    sh_result[8] = 0.54627f * (n.x * n.x - n.y * n.y) * sh.coefs[8];

    vec3 col = vec3(0.0);
    for (int i = 0; i < 9; ++i)
    {
        col += sh_result[i];
    }
    col = max(vec3(0.0), col);
    return col;
}

#endif