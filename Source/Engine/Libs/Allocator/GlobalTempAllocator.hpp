#pragma once
#include <cstddef>

template <typename T>
class GlobalTempAllocator : public std::allocator<T> {
//public:
//    // Type definitions required by the standard library
//    using value_type = T;
//    using pointer = T*;
//    using const_pointer = const T*;
//    using size_type = std::size_t;
//    using difference_type = std::ptrdiff_t;
//
//    // Allocate memory
//    T* allocate(std::size_t n) {
//        return static_cast<T*>(::operator new(n * sizeof(T)));
//    }
//
//    // Deallocate memory
//    void deallocate(T* p, std::size_t n) {
//        ::operator delete(p);
//    }
};
