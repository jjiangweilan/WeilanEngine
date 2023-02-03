#pragma once

namespace Engine
{
// https : stackoverflow.com/questions/7666509/hash-function-for-string
typedef unsigned long ID;
constexpr ID _ID(const char* str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}
} // namespace Engine
