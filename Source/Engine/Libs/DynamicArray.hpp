#pragma once
#include "Memory.hpp"
#include <vector>

template <class T, class Allocator = GlobalMemoryAllocator<T>>
using DynamicArray = std::vector<T, Allocator>;
