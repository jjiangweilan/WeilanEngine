#include <gtest/gtest.h>
#include "Engine/Core/ObjectTracker.hpp"
#include "Engine/Core/Object.hpp"
#include <thread>
#include <vector>
#include <atomic>

class TestObject : public Object {
public:
    TestObject(const UUID& uuid) { this->uuid = uuid; }
};

TEST(ObjectTrackerTest, BasicAddRemove) {
    ObjectTracker& tracker = ObjectTracker::Singleton();
    UUID uuid = UUID();
    TestObject* obj = new TestObject(uuid);

    tracker.AddObject(obj);
    ObjectTrackHandle handle = tracker.Track(uuid);
    
    EXPECT_EQ(tracker.GetObject(handle), obj);
    
    tracker.RemoveObject(obj);
    EXPECT_EQ(tracker.GetObject(handle), nullptr);
    
    tracker.Detrack(handle);
    delete obj;
}

TEST(ObjectTrackerTest, PaginatedAllocation) {
    ObjectTracker& tracker = ObjectTracker::Singleton();
    const int count = 10000; // More than one page (4096)
    std::vector<TestObject*> objects;
    std::vector<ObjectTrackHandle> handles;

    for (int i = 0; i < count; ++i) {
        UUID uuid = UUID();
        TestObject* obj = new TestObject(uuid);
        objects.push_back(obj);
        tracker.AddObject(obj);
        handles.push_back(tracker.Track(uuid));
    }

    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(tracker.GetObject(handles[i]), objects[i]);
    }

    for (int i = 0; i < count; ++i) {
        tracker.RemoveObject(objects[i]);
        tracker.Detrack(handles[i]);
        delete objects[i];
    }
}

TEST(ObjectTrackerTest, ConcurrentAddTrack) {
    ObjectTracker& tracker = ObjectTracker::Singleton();
    const int numThreads = 8;
    const int objectsPerThread = 1000;
    std::vector<std::thread> threads;
    
    struct ThreadData {
        std::vector<TestObject*> objects;
        std::vector<ObjectTrackHandle> handles;
    };
    std::vector<ThreadData> allData(numThreads);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&tracker, &data = allData[i], objectsPerThread]() {
            for (int j = 0; j < objectsPerThread; ++j) {
                UUID uuid = UUID();
                TestObject* obj = new TestObject(uuid);
                data.objects.push_back(obj);
                tracker.AddObject(obj);
                data.handles.push_back(tracker.Track(uuid));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    for (int i = 0; i < numThreads; ++i) {
        for (int j = 0; j < objectsPerThread; ++j) {
            EXPECT_EQ(tracker.GetObject(allData[i].handles[j]), allData[i].objects[j]);
        }
    }

    // Cleanup
    for (int i = 0; i < numThreads; ++i) {
        for (int j = 0; j < objectsPerThread; ++j) {
            tracker.RemoveObject(allData[i].objects[j]);
            tracker.Detrack(allData[i].handles[j]);
            delete allData[i].objects[j];
        }
    }
}

TEST(ObjectTrackerTest, ReplaceObject) {
    ObjectTracker& tracker = ObjectTracker::Singleton();
    UUID uuid = UUID();
    TestObject* obj1 = new TestObject(uuid);
    tracker.AddObject(obj1);
    ObjectTrackHandle handle = tracker.Track(uuid);

    TestObject* obj2 = new TestObject(UUID::GetEmptyUUID());
    tracker.ReplaceObject(obj2, obj1);

    EXPECT_EQ(tracker.GetObject(handle), obj2);
    EXPECT_EQ(obj2->GetUUID(), uuid);
    EXPECT_TRUE(obj1->GetUUID().IsEmpty());

    tracker.RemoveObject(obj2);
    tracker.Detrack(handle);
    delete obj1;
    delete obj2;
}
