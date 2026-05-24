#pragma once

#include <cstdint>

namespace Gfx
{

class Buffer;

enum class TemporaryBufferUsage
{
    Uniform,
    Storage,
    Index,
    Indirect,
    TransferSrc,
    AccelerationStructure,
};

struct TemporaryBufferHandle
{
    uint64_t id = 0;
    uint64_t size = 0;
    TemporaryBufferUsage usage = TemporaryBufferUsage::Storage;

    bool IsValid() const { return id != 0; }
};

struct BufferIdentifier
{
    enum class Type
    {
        None,
        RawBuffer,
        TemporaryBuffer,
    };

    Type type = Type::None;
    Gfx::Buffer* buffer = nullptr;
    TemporaryBufferHandle temporaryBuffer{};

    BufferIdentifier() = default;
    BufferIdentifier(Gfx::Buffer& buf) : type(Type::RawBuffer), buffer(&buf) {}
    BufferIdentifier(Gfx::Buffer* buf) : type(buf ? Type::RawBuffer : Type::None), buffer(buf) {}
    BufferIdentifier(TemporaryBufferHandle tb) : type(tb.IsValid() ? Type::TemporaryBuffer : Type::None), temporaryBuffer(tb) {}

    bool IsBuffer() const { return type != Type::None; }
    bool IsValid() const { return type == Type::RawBuffer ? buffer != nullptr : (type == Type::TemporaryBuffer && temporaryBuffer.IsValid()); }
    bool IsTemporary() const { return type == Type::TemporaryBuffer; }
    bool IsRawBuffer() const { return type == Type::RawBuffer; }
};

struct BufferHandle
{
    void* mappedData = nullptr;
    uint64_t deviceAddress = 0;
    uint64_t offset = 0;
    uint64_t size = 0;
};

} // namespace Gfx
