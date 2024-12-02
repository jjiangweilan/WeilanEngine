#pragma once
#include "Libs/UUID.hpp"
#include <vector>
#include <unordered_map>

class Object;
class ObjectTracker
{
public:
    ObjectTracker()
    {
        slots.reserve(1024);
        freeSlotIndices.reserve(1024);
    }

    void AddRef(const UUID& uuid);
    void RemoveRef(const UUID& uuid);
    void Track(Object* object);
    void Detrack(Object* object);

private:
    struct Slot
    {
        Object* object;
        uint32_t referenceCount;
    };

    std::vector<Slot> slots;
    std::vector<uint32_t> freeSlotIndices;
    std::unordered_map<UUID, uint32_t> uuidToSlotIndex;
    uint32_t nextFreeSlot = 0;

    uint32_t AllocateSlot();
    void PushbackToFreeSlotIndicesIfNotReferenced(uint32_t slotIndex);
};
