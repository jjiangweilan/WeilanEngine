#pragma once

class Memory
{};

// fast, thread local memory allocation
// should be used for small allocations within a function scope
class StackMemoryAllocator
{};

// using shared memory within a frame
// ideally to be used for temporary allocation that need to communicate within a frame
// reset at the end of the frame
class FrameMemoryAllocator
{};

// slow, but not restricted in size. Manual deallocation required
// designed to have low fragmentation
class PermenentMemoryAllocator
{};
