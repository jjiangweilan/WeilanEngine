#pragma once

#include "Engine/Library/UUID.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>

class ModelSubAssetUUIDAllocator
{
public:
    void Reset(std::unordered_map<std::string, UUID>* nameToUUID)
    {
        this->nameToUUID = nameToUUID;
        knownUUIDs.clear();
        assignedUUIDs.clear();

        if (nameToUUID != nullptr)
        {
            for (const auto& [key, uuid] : *nameToUUID)
            {
                (void)key;
                if (!uuid.IsEmpty())
                {
                    knownUUIDs.insert(uuid);
                }
            }
        }
    }

    UUID GetOrCreate(const std::string& key)
    {
        if (nameToUUID != nullptr)
        {
            auto iter = nameToUUID->find(key);
            if (iter != nameToUUID->end() && !iter->second.IsEmpty() && !assignedUUIDs.contains(iter->second))
            {
                assignedUUIDs.insert(iter->second);
                return iter->second;
            }
        }

        UUID uuid;
        while (knownUUIDs.contains(uuid))
        {
            uuid = UUID();
        }

        knownUUIDs.insert(uuid);
        assignedUUIDs.insert(uuid);
        if (nameToUUID != nullptr)
        {
            (*nameToUUID)[key] = uuid;
        }
        return uuid;
    }

private:
    std::unordered_map<std::string, UUID>* nameToUUID = nullptr;
    std::unordered_set<UUID> knownUUIDs;
    std::unordered_set<UUID> assignedUUIDs;
};
