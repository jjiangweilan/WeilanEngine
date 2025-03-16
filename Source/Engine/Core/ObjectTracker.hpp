#pragma once
#include "Libs/SpinLock.hpp"
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
        uuidToSlotIndex[UUID::GetEmptyUUID()] = 0;
    }

    ~ObjectTracker();

    static ObjectTracker& Singleton();

    inline Object* GetObject(ObjectTrackHandle handle)
    {
        ScopedSpinLock lk{lock};
        return slots.at(handle).object;
    }

    ObjectTrackHandle Track(const UUID& uuid);
    ObjectTrackHandle Track(ObjectTrackHandle handle);
    void Detrack(ObjectTrackHandle handle);
    void Detrack(const UUID& uuid);

    void AddObject(Object* object);
    void RemoveObject(Object* object);
    void ReplaceObjectUUID(Object* object, const UUID& uuid);
    void ReplaceObject(Object* dst, Object* src);

    std::unordered_map<UUID, ObjectTrackHandle> GetUUIDToSlotIndex()
    {
        ScopedSpinLock lk{lock};
        return uuidToSlotIndex;
    }

private:
    struct Slot
    {
        Object* object = nullptr;
        uint32_t referenceCount = 0;
    };

    std::vector<Slot> slots;
    std::vector<uint32_t> freeSlotIndices;
    std::unordered_map<uint32_t, UUID> slotIndexToUUID;
    std::unordered_map<UUID, uint32_t> uuidToSlotIndex;
    Spinlock lock;

    uint32_t GetOrAllocateSlot(const UUID& uuid);
    uint32_t AllocateSlot();
    void ReleaseSlotIfNotReferenced(uint32_t slotIndex);
    inline void RemoveObjectImpl(Object* object);
    inline void AddObjectImpl(Object* object);
};
