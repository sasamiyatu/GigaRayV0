#ifndef RANDOM_H
#define RANDOM_H

uvec4 pcg4d(inout uvec4 v)
{
    v = v * 1664525u + 1013904223u;
    v += v.yzxy * v.wxyz;
    v = v ^ (v >> 16u);
    v += v.yzxy * v.wxyz;

    return v;
}

#endif