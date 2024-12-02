#include "ObjectTracker.hpp"
#include "Libs/Assert.hpp"
#include "Object.hpp"

void ObjectTracker::Track(Object* object)
{
#if ENGINE_DEV_BUILD
    if (uuidToSlotIndex.find(object->GetUUID()) != uuidToSlotIndex.end())
    {
        spdlog::error("making object with duplicated UUID");
    }
    else
#endif
    {
        uint32_t slotIndex = AllocateSlot();
        slots[slotIndex].object = object;
        ASSERT(slots[slotIndex].referenceCount == 0);
    }
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

void ObjectTracker::Detrack(Object* object)
{
    auto uuidToSlotIndexIter = uuidToSlotIndex.find(object->GetUUID());
    ASSERT(uuidToSlotIndexIter != uuidToSlotIndex.end());

    uint32_t slotIndex = uuidToSlotIndexIter->second;
    ASSERT(slotIndex >= 0 && slotIndex < slots.size());

    slots[slotIndex].object = nullptr;
    PushbackToFreeSlotIndicesIfNotReferenced(slotIndex);
}
