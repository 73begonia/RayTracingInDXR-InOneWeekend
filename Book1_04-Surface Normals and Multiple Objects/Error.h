#pragma once
#include "pch.h"

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw hr;
	}
}

inline void ThrowIfFalse(bool value)
{
	ThrowIfFailed(value ? S_OK : E_FAIL);
}

inline void printError(const char* errorMessage)
{
	if (errorMessage)
		puts(errorMessage);
}

class Error
{
public:
	Error() {}
	Error(const char* errorMessage)
	{
		printError(errorMessage);
	}
};