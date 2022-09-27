#pragma once
#include <cinttypes>
#include <memory>
#include <functional>
#include "Internal/uuids/uuid.h"
namespace Engine
{
    class UUID
    {
        public:
            UUID();
            UUID(const UUID& uuid);
            UUID(const std::string& uuid);
           ~UUID();

            bool operator==(const UUID& other) const { return id == other.id; }
            bool operator!=(const UUID& other) const { return id != other.id; }
            const UUID& operator=(const UUID& other);
            bool IsEmpty() const;
            const std::string& ToString() const;
            static const UUID empty;
        private:

            UUID(bool empty);
            static std::mt19937 generator;
            uuids::uuid id;
            std::string strID;

            friend class std::hash<UUID>;
    };
}
template<>
struct std::hash<Engine::UUID>
{
    std::size_t operator()(Engine::UUID const& s) const noexcept
    {
        return std::hash<uuids::uuid>{}(s.id);
    }
};