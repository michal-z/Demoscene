#pragma once

#include "tiny_obj_loader.h"


namespace Lib
{

std::vector<uint8_t> LoadFile(const char* fileName);
double GetTime();
float Randomf();
float Randomf(float begin, float end);

}

// returns [0.0f, 1.0f)
inline float Lib::Randomf()
{
	const uint32_t exponent = 127;
	const uint32_t significand = (uint32_t)(rand() & 0x7fff); // get 15 random bits
	const uint32_t result = (exponent << 23) | (significand << 8);
	return *(float*)&result - 1.0f;
}

// returns [begin, end)
inline float Lib::Randomf(float begin, float end)
{
	assert(begin < end);
	return begin + (end - begin) * Randomf();
}
