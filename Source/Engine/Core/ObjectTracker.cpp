#include "ObjectTracker.hpp"
#include "Engine/Core/Asset.hpp"
#include "Engine/Library/Assert.hpp"
#include "Object.hpp"

ObjectTracker::ObjectTracker()
{
    globalIndexCounter.store(1, std::memory_order_relaxed); // 0 is NullHandle
    for (uint32_t i = 0; i < MAX_PAGES; ++i)
    {
        pages[i].store(nullptr, std::memory_order_relaxed);
    }
    
    // Ensure page 0 is allocated for NullHandle (index 0) and any immediate allocations
    EnsurePage(0);
    
    // Assign NullHandle mapping in shard map
    UUID emptyUUID = UUID::GetEmptyUUID();
    size_t shardIdx = std::hash<UUID>()(emptyUUID) % SHARD_COUNT;
    shards[shardIdx].map[emptyUUID] = 0;
}

ObjectTracker::~ObjectTracker()
{
    for (uint32_t i = 0; i < MAX_PAGES; ++i)
    {
        Page* p = pages[i].load(std::memory_order_relaxed);
        if (p)
        {
            delete p;
        }
    }
}

ObjectTracker& ObjectTracker::Singleton()
{
    static ObjectTracker singleton;
    return singleton;
}

void ObjectTracker::EnsurePage(uint32_t index)
{
    uint32_t pageIndex = index / PAGE_SIZE;
    ASSERT(pageIndex < MAX_PAGES);

    Page* p = pages[pageIndex].load(std::memory_order_acquire);
    if (p == nullptr)
    {
        Page* newPage = new Page();
        Page* expected = nullptr;
        if (pages[pageIndex].compare_exchange_strong(expected, newPage, std::memory_order_release, std::memory_order_acquire))
        {
            // Successfully set the new page
        }
        else
        {
            // Another thread set it, delete ours
            delete newPage;
        }
    }
}

uint32_t ObjectTracker::GetOrAllocateSlot(const UUID& uuid, bool incrementRefCount)
{
    size_t shardIndex = std::hash<UUID>()(uuid) % SHARD_COUNT;
    Shard& shard = shards[shardIndex];

    {
        ScopedSpinLock lk(shard.lock);
        uint32_t slot;
        auto iter = shard.map.find(uuid);
        if (iter != shard.map.end())
        {
            slot = iter->second;
        }
        else
        {
            slot = globalIndexCounter.fetch_add(1, std::memory_order_relaxed);
            shard.map[uuid] = slot;
            EnsurePage(slot);
            uint32_t pageIdx = slot / PAGE_SIZE;
            uint32_t localOffset = slot % PAGE_SIZE;
            pages[pageIdx].load(std::memory_order_relaxed)->uuids[localOffset] = uuid;
        }

        if (incrementRefCount && slot != NullHandle)
        {
            uint32_t pageIdx = slot / PAGE_SIZE;
            uint32_t localOffset = slot % PAGE_SIZE;
            pages[pageIdx].load(std::memory_order_relaxed)->refCounts[localOffset].fetch_add(1, std::memory_order_relaxed);
        }

        return slot;
    }
}

void ObjectTracker::ReleaseSlotIfNotReferenced(uint32_t slotIndex)
{
    if (slotIndex == NullHandle) return;

    uint32_t pageIdx = slotIndex / PAGE_SIZE;
    uint32_t localOffset = slotIndex % PAGE_SIZE;
    Page* page = pages[pageIdx].load(std::memory_order_acquire);
    
    if (!page) return;

    UUID uuid = page->uuids[localOffset];
    if (uuid.IsEmpty()) return;

    size_t shardIndex = std::hash<UUID>()(uuid) % SHARD_COUNT;
    ScopedSpinLock lk(shards[shardIndex].lock);
    
    if (page->refCounts[localOffset].load(std::memory_order_acquire) == 0 && 
        page->objects[localOffset].load(std::memory_order_acquire) == nullptr)
    {
        auto iter = shards[shardIndex].map.find(uuid);
        if (iter != shards[shardIndex].map.end() && iter->second == slotIndex)
        {
            shards[shardIndex].map.erase(iter);
        }
    }
}

void ObjectTracker::AddObjectImpl(Object* object)
{
    const UUID& uuid = object->GetUUID();
    if (uuid.IsEmpty())
        return;

    uint32_t slotIndex = GetOrAllocateSlot(uuid);
    
    uint32_t pageIdx = slotIndex / PAGE_SIZE;
    uint32_t localOffset = slotIndex % PAGE_SIZE;
    Page* page = pages[pageIdx].load(std::memory_order_acquire);
    
    ASSERT(page->objects[localOffset].load(std::memory_order_relaxed) == nullptr);
    page->objects[localOffset].store(object, std::memory_order_release);
}

void ObjectTracker::AddObject(Object* object)
{
    AddObjectImpl(object);
}

void ObjectTracker::RemoveObjectImpl(Object* object)
{
    const UUID& uuid = object->GetUUID();
    if (uuid.IsEmpty())
        return;

    size_t shardIndex = std::hash<UUID>()(uuid) % SHARD_COUNT;
    uint32_t slotIndex = NullHandle;
    
    {
        ScopedSpinLock lk(shards[shardIndex].lock);
        auto iter = shards[shardIndex].map.find(uuid);
        if (iter != shards[shardIndex].map.end())
        {
            slotIndex = iter->second;
        }
    }

    if (slotIndex != NullHandle)
    {
        uint32_t pageIdx = slotIndex / PAGE_SIZE;
        uint32_t localOffset = slotIndex % PAGE_SIZE;
        Page* page = pages[pageIdx].load(std::memory_order_acquire);
        
        if (page)
        {
            page->objects[localOffset].store(nullptr, std::memory_order_release);
            ReleaseSlotIfNotReferenced(slotIndex);
        }
    }
}

void ObjectTracker::RemoveObject(Object* object)
{
    RemoveObjectImpl(object);
}

void ObjectTracker::ReplaceObject(Object* dst, Object* src)
{
    RemoveObjectImpl(src);
    dst->uuid = std::exchange(src->uuid, UUID::GetEmptyUUID());
    AddObjectImpl(dst);
}

void ObjectTracker::ReplaceObjectUUID(Object* object, const UUID& uuid)
{
    RemoveObjectImpl(object);
    object->uuid = uuid;
    AddObjectImpl(object);
}

ObjectTrackHandle ObjectTracker::Track(ObjectTrackHandle handle)
{
    if (handle != NullHandle)
    {
        uint32_t pageIdx = handle / PAGE_SIZE;
        uint32_t localOffset = handle % PAGE_SIZE;
        Page* page = pages[pageIdx].load(std::memory_order_acquire);
        if (page)
        {
            page->refCounts[localOffset].fetch_add(1, std::memory_order_acq_rel);
        }
    }
    return handle;
}

ObjectTrackHandle ObjectTracker::Track(const UUID& uuid)
{
    uint32_t slotIndex = GetOrAllocateSlot(uuid, true);
    return slotIndex;
}

void ObjectTracker::Detrack(ObjectTrackHandle handle)
{
    if (handle != NullHandle)
    {
        uint32_t pageIdx = handle / PAGE_SIZE;
        uint32_t localOffset = handle % PAGE_SIZE;
        Page* page = pages[pageIdx].load(std::memory_order_acquire);
        if (page)
        {
            uint32_t refs = page->refCounts[localOffset].fetch_sub(1, std::memory_order_acq_rel);
            ASSERT(refs > 0);
            ReleaseSlotIfNotReferenced(handle);
        }
    }
}

void ObjectTracker::Detrack(const UUID& uuid)
{
    if (uuid.IsEmpty())
        return;

    size_t shardIndex = std::hash<UUID>()(uuid) % SHARD_COUNT;
    uint32_t slotIndex = NullHandle;
    
    {
        ScopedSpinLock lk(shards[shardIndex].lock);
        auto iter = shards[shardIndex].map.find(uuid);
        if (iter != shards[shardIndex].map.end())
        {
            slotIndex = iter->second;
        }
    }

    if (slotIndex != NullHandle)
    {
        uint32_t pageIdx = slotIndex / PAGE_SIZE;
        uint32_t localOffset = slotIndex % PAGE_SIZE;
        Page* page = pages[pageIdx].load(std::memory_order_acquire);
        if (page)
        {
            uint32_t refs = page->refCounts[localOffset].fetch_sub(1, std::memory_order_acq_rel);
            ASSERT(refs > 0);
            ReleaseSlotIfNotReferenced(slotIndex);
        }
    }
}

boost::unordered_flat_map<UUID, ObjectTrackHandle, std::hash<UUID>> ObjectTracker::GetUUIDToSlotIndex()
{
    boost::unordered_flat_map<UUID, ObjectTrackHandle, std::hash<UUID>> result;
    for (uint32_t i = 0; i < SHARD_COUNT; ++i)
    {
        ScopedSpinLock lk(shards[i].lock);
        for (const auto& pair : shards[i].map)
        {
            result[pair.first] = pair.second;
        }
    }
    return result;
}
