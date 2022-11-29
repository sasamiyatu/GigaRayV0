#ifndef COLOR_H
#define COLOR_H

float luminance(vec3 rgb)
{
	return dot(rgb, vec3(0.2126f, 0.7152f, 0.0722f));
}

#endif