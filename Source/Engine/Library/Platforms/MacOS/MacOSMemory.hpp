#pragma once

#if __APPLE__

#include <cstddef>

void* Platform_AllocateMemory(size_t size, bool tls, const char* allocationTag);
void Platform_FreeMemory(void* ptr, bool tls);
void* Platform_ReAllocateMemory(void* ptr, size_t newSize, bool tls);

#endif
