#pragma once
#include "Memory.hpp"
#include <vector>

template <class T, class Allocator = std::allocator<T>>
using DynamicArray = std::vector<T, Allocator>;

