#include "ObjectTracker.hpp"
#include "Core/Asset.hpp"
#include "Libs/Assert.hpp"
#include "Object.hpp"
ObjectTracker& ObjectTracker::Singleton()
{
    static ObjectTracker singleton;
    return singleton;
}

void ObjectTracker::AddObject(Object* object)
{
    ScopedSpinLock lk{lock};

    const UUID& uuid = object->GetUUID();
    if (uuid.IsEmpty())
        return;

    uint32_t slotIndex = GetOrAllocateSlot(uuid);

    ASSERT(slots[slotIndex].object == nullptr);

    slots[slotIndex].object = object;
}

void ObjectTracker::RemoveObject(Object* object)
{
    ScopedSpinLock lk{lock};
    if (object->GetUUID().IsEmpty())
        return;

    auto uuidToSlotIndexIter = uuidToSlotIndex.find(object->GetUUID());
    ASSERT(uuidToSlotIndexIter != uuidToSlotIndex.end());

    uint32_t slotIndex = uuidToSlotIndexIter->second;

    if (slotIndex != NullHandle)
    {
        ASSERT(slotIndex > 0 && slotIndex < slots.size());
        slots[slotIndex].object = nullptr;
        ReleaseSlotIfNotReferenced(slotIndex);
    }
}

ObjectTrackHandle ObjectTracker::Track(ObjectTrackHandle handle)
{
    ScopedSpinLock lk{lock};
    uint32_t slotIndex = handle;
    slots[slotIndex].referenceCount += 1;

    return slotIndex;
}

ObjectTrackHandle ObjectTracker::Track(const UUID& uuid)
{
    ScopedSpinLock lk{lock};
    uint32_t slotIndex = GetOrAllocateSlot(uuid);
    slots[slotIndex].referenceCount += 1;

    return slotIndex;
}

void ObjectTracker::Detrack(ObjectTrackHandle handle)
{
    ScopedSpinLock lk{lock};
    if (handle != NullHandle)
    {
        uint32_t slotIndex = handle;
        ASSERT(slots[slotIndex].referenceCount != 0);
        slots[slotIndex].referenceCount -= 1;
        ReleaseSlotIfNotReferenced(slotIndex);
    }
}

void ObjectTracker::Detrack(const UUID& uuid)
{
    ScopedSpinLock lk{lock};
    auto iter = uuidToSlotIndex.find(uuid);

    if (iter != uuidToSlotIndex.end())
    {
        auto slotIndex = iter->second;
        if (slotIndex != 0)
        {
            ASSERT(slots[slotIndex].referenceCount != 0);
            slots[slotIndex].referenceCount -= 1;
            ReleaseSlotIfNotReferenced(slotIndex);
        }
    }
}

void ObjectTracker::ReleaseSlotIfNotReferenced(uint32_t slotIndex)
{
    if (slots[slotIndex].referenceCount == 0 && slots[slotIndex].object == nullptr)
    {
        const auto& uuid = slotIndexToUUID[slotIndex];
        uuidToSlotIndex.erase(uuid);
        slotIndexToUUID.erase(slotIndex);
        freeSlotIndices.push_back(slotIndex);
    }
}

uint32_t ObjectTracker::GetOrAllocateSlot(const UUID& uuid)
{
    auto iter = uuidToSlotIndex.find(uuid);

    if (iter != uuidToSlotIndex.end())
        return iter->second;

    uint32_t newSlot = AllocateSlot();
    uuidToSlotIndex[uuid] = newSlot;
    slotIndexToUUID[newSlot] = uuid;
    return newSlot;
}

uint32_t ObjectTracker::AllocateSlot()
{
    uint32_t slotIndex = -1;
    if (freeSlotIndices.empty())
    {
        slots.push_back({nullptr, 0});
        slotIndex = slots.size() - 1;
    }
    else
    {
        slotIndex = freeSlotIndices.back();
        freeSlotIndices.pop_back();
    }

    return slotIndex;
}
