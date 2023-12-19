#pragma once
#include <stdint.h>
namespace Gfx
{
class StorageBuffer
{
public:
    virtual void UpdateData(void* data) = 0;
    virtual uint32_t GetSize() = 0;
};
} // namespace Gfx
