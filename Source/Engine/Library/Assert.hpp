#pragma once

// TODO: causing JPH defines AsserFailed
// #ifdef NDEBUG
// #define NDEBUG_DEFINED
// #endif
//
// #if ENGINE_DEV_BUILD
// #undef NDEBUG
// #endif

#include <cstdlib>
#define ASSERT(expression)                                                                                             \
    if (!(expression))                                                                                                 \
    {                                                                                                                  \
        std::abort();                                                                                                  \
    }

// #if defined(rDEBUG_DEFINED) && defined(ENGINE_DEV_BUILD) && !defined(NDEBUG)
// #define NDEBUG 1
// #endif
