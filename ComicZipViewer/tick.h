#pragma once
#include "framework.h"

inline int64_t GetTickFrequency()
{
	LARGE_INTEGER largeInteger{};
	QueryPerformanceFrequency(&largeInteger);
	return largeInteger.QuadPart;
}

inline int64_t GetTick()
{
	LARGE_INTEGER largeInteger{};
	QueryPerformanceCounter(&largeInteger);
	return largeInteger.QuadPart;
}
