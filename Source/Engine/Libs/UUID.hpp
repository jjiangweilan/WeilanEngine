#pragma once
#include "Internal/uuids/uuid.h"
#include <cinttypes>
#include <functional>
#include <memory>
class UUID
{
public:
    struct FromStrTag
    {};
    UUID();
    UUID(const UUID& uuid);
    UUID(UUID&& other) = default;
    UUID(const std::string& uuid);
    UUID(const std::string& str, FromStrTag);
    UUID(const char* uuid);
    ~UUID();

    bool operator==(const UUID& other) const
    {
        return id == other.id;
    }
    bool operator!=(const UUID& other) const
    {
        return id != other.id;
    }
    const UUID& operator=(const UUID& other);
    bool IsEmpty() const;
    const std::string& ToString() const;
    static const UUID& GetEmptyUUID();

private:
    struct EmptyTag
    {};
    UUID(EmptyTag);
    static std::mt19937 generator;
    static uuids::uuid_name_generator nameGenerator;
    uuids::uuid id;
    std::string strID;

    friend class std::hash<UUID>;
};

//
template <>
struct std::hash<UUID>
{
    std::size_t operator()(UUID const& s) const noexcept
    {
        return std::hash<uuids::uuid>{}(s.id);
    }
};
