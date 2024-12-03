#include "ObjectTracker.hpp"
#include "Libs/Assert.hpp"
#include "Object.hpp"

void ObjectTracker::AddObject(Object* object)
{
    uint32_t slotIndex = GetOrAllocateSlot(object->GetUUID());
    ASSERT(slots[slotIndex].object == nullptr);
    ASSERT(slots[slotIndex].referenceCount == 0);

    slots[slotIndex].object = object;
}

void ObjectTracker::RemoveObject(Object* object)
{
    auto uuidToSlotIndexIter = uuidToSlotIndex.find(object->GetUUID());
    ASSERT(uuidToSlotIndexIter != uuidToSlotIndex.end());

    uint32_t slotIndex = uuidToSlotIndexIter->second;
    ASSERT(slotIndex >= 0 && slotIndex < slots.size());

    slots[slotIndex].object = nullptr;
    ReleaseSlotIfNotReferenced(slotIndex);
}

ObjectTrackHandle ObjectTracker::Track(ObjectTrackHandle handle)
{
    uint32_t slotIndex = handle;
    slots[slotIndex].referenceCount += 1;

    return slotIndex;
}

ObjectTrackHandle ObjectTracker::Track(const UUID& uuid)
{
    uint32_t slotIndex = GetOrAllocateSlot(uuid);
    slots[slotIndex].referenceCount += 1;

    return slotIndex;
}

void ObjectTracker::Detrack(ObjectTrackHandle handle)
{
    if (handle != NullHandle)
    {
        uint32_t slotIndex = handle;
        ASSERT(slots[slotIndex].referenceCount != 0);
        slots[slotIndex].referenceCount -= 1;
        ReleaseSlotIfNotReferenced(slotIndex);
    }
}

void ObjectTracker::ReleaseSlotIfNotReferenced(uint32_t slotIndex)
{
    if (slots[slotIndex].referenceCount == 0)
    {
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
    return newSlot;
}

uint32_t ObjectTracker::AllocateSlot()
{
    uint32_t slotIndex = -1;
    if (freeSlotIndices.empty())
    {
        if (nextFreeSlot >= slots.size())
        {
            slots.reserve(slots.size() * 2);
        }

        slotIndex = nextFreeSlot;
        nextFreeSlot++;
        return slotIndex;
    }
    else
    {
        slotIndex = freeSlotIndices.back();
        freeSlotIndices.pop_back();
    }

    return slotIndex;
}
