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
        freeSlotIndex.reserve(1024);
    }

    uint32_t Track(Object* object);
    void Detrack(Object* object);

private:
    struct Slot
    {
        Object* object;
        uint32_t referenceCount;
    };

    std::vector<Slot> slots;
    std::vector<uint32_t> freeSlotIndex;
    std::unordered_map<UUID, uint32_t> uuidToSlotIndex;
    uint32_t nextFreeSlot = 0;

    uint32_t AllocateSlot();
};
