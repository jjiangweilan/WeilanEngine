#pragma once
#include "Libs/UUID.hpp"
#include <unordered_map>
#include <vector>

class Object;
using ObjectTrackHandle = uint32_t;
class ObjectTracker
{
public:
    static const ObjectTrackHandle NullHandle = 0;

    ObjectTracker()
    {
        slots.reserve(1024);
        freeSlotIndices.reserve(1024);

        slots.push_back({nullptr, 1}); // the first slot is used for nullptr
    }

    inline static ObjectTracker& Singleton()
    {
        static ObjectTracker singleton;
        return singleton;
    }

    inline Object* GetObject(ObjectTrackHandle handle) { return slots.at(handle).object; }

    ObjectTrackHandle Track(const UUID& uuid);
    ObjectTrackHandle Track(ObjectTrackHandle handle);
    void Detrack(ObjectTrackHandle handle);

    void AddObject(Object* object);
    void RemoveObject(Object* object);

private:
    struct Slot
    {
        Object* object = nullptr;
        uint32_t referenceCount = 0;
    };

    std::vector<Slot> slots;
    std::vector<uint32_t> freeSlotIndices;
    std::unordered_map<UUID, uint32_t> uuidToSlotIndex;
    uint32_t nextFreeSlot = 0;

    uint32_t GetOrAllocateSlot(const UUID& uuid);
    uint32_t AllocateSlot();
    void ReleaseSlotIfNotReferenced(uint32_t slotIndex);
};
