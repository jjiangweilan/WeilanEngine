#pragma once

#if ENGINE_DEV_BUILD
#undef NDEBUG
#endif

#include "assert.h"

#define ASSERT(expression) assert(expression)
