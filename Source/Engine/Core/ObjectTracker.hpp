#pragma once
#include "Engine/Library/SpinLock.hpp"
#include "Engine/Library/UUID.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <unordered_map>
#include "Engine/Library/DynamicArray.hpp"
#include <atomic>
#include <mutex>
#include <vector>

class Object;
using ObjectTrackHandle = uint32_t;

class ObjectTracker
{
public:
    static const ObjectTrackHandle NullHandle = 0;

    ObjectTracker();
    ~ObjectTracker();

    static ObjectTracker& Singleton();

    inline Object* GetObject(ObjectTrackHandle handle)
    {
        uint32_t pageIndex = handle / PAGE_SIZE;
        uint32_t localOffset = handle % PAGE_SIZE;
        Page* page = pages[pageIndex].load(std::memory_order_acquire);
        if (!page) return nullptr;
        return page->objects[localOffset].load(std::memory_order_acquire);
    }

    ObjectTrackHandle Track(const UUID& uuid);
    ObjectTrackHandle Track(ObjectTrackHandle handle);
    void Detrack(ObjectTrackHandle handle);
    void Detrack(const UUID& uuid);

    void AddObject(Object* object);
    void RemoveObject(Object* object);
    void ReplaceObjectUUID(Object* object, const UUID& uuid);
    void ReplaceObject(Object* dst, Object* src);

    boost::unordered_flat_map<UUID, ObjectTrackHandle, std::hash<UUID>> GetUUIDToSlotIndex();

private:
    static constexpr uint32_t PAGE_SIZE = 4096;
    static constexpr uint32_t MAX_PAGES = 16384; // ~67 million objects max
    static constexpr uint32_t SHARD_COUNT = 128;

    struct Page
    {
        std::atomic<Object*> objects[PAGE_SIZE];
        std::atomic<uint32_t> refCounts[PAGE_SIZE];
        UUID uuids[PAGE_SIZE];

        Page()
        {
            for (uint32_t i = 0; i < PAGE_SIZE; ++i)
            {
                objects[i].store(nullptr, std::memory_order_relaxed);
                refCounts[i].store(0, std::memory_order_relaxed);
            }
        }
    };

    struct Shard
    {
        Spinlock lock;
        boost::unordered_flat_map<UUID, uint32_t, std::hash<UUID>> map;
    };

    std::atomic<Page*> pages[MAX_PAGES];
    Shard shards[SHARD_COUNT];
    std::atomic<uint32_t> globalIndexCounter;

    uint32_t GetOrAllocateSlot(const UUID& uuid, bool incrementRefCount = false);
    void EnsurePage(uint32_t index);
    void ReleaseSlotIfNotReferenced(uint32_t slotIndex);
    inline void RemoveObjectImpl(Object* object);
    inline void AddObjectImpl(Object* object);
};
