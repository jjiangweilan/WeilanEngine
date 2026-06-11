#include "Engine/Runtime/System/SceneManager/RenderingObjectList.hpp"
#include <algorithm>
#include <gtest/gtest.h>

namespace
{
class TestRenderingObject : public RenderingObjectBase
{
public:
    explicit TestRenderingObject(Rendering::RenderEvents event = Rendering::RenderEvents::None)
    {
        SetRenderEvent(event);
    }
};

bool Contains(RenderingObjectList::ObjectList objects, RenderingObjectBase* object)
{
    return std::find(objects.begin(), objects.end(), object) != objects.end();
}
} // namespace

TEST(RenderingObjectListTest, RemovesObjectsByIdentityAfterSwaps)
{
    RenderingObjectList list;
    TestRenderingObject first;
    TestRenderingObject middle;
    TestRenderingObject last;

    list.AddToList(0, &first);
    list.AddToList(0, &middle);
    list.AddToList(0, &last);

    list.RemoveFromList(0, &first);
    auto objects = list.GetRenderingObjects(0);
    ASSERT_EQ(objects.size(), 2);
    EXPECT_FALSE(Contains(objects, &first));
    EXPECT_TRUE(Contains(objects, &middle));
    EXPECT_TRUE(Contains(objects, &last));

    list.RemoveFromList(0, &last);
    objects = list.GetRenderingObjects(0);
    ASSERT_EQ(objects.size(), 1);
    EXPECT_EQ(objects.front(), &middle);

    list.RemoveFromList(0, &middle);
    EXPECT_TRUE(list.GetRenderingObjects(0).empty());
}

TEST(RenderingObjectListTest, RemovesOnlyMatchingObjectFromSharedEventList)
{
    RenderingObjectList list;
    TestRenderingObject deferredTypeZero(Rendering::RenderEvents::Deferred);
    TestRenderingObject deferredTypeOne(Rendering::RenderEvents::Deferred);

    list.AddToList(0, &deferredTypeZero);
    list.AddToList(1, &deferredTypeOne);

    list.RemoveFromList(1, &deferredTypeOne);
    auto eventObjects = list.GetRenderingObjectsByEvent(Rendering::RenderEvents::Deferred);
    ASSERT_EQ(eventObjects.size(), 1);
    EXPECT_EQ(eventObjects.front(), &deferredTypeZero);
    EXPECT_TRUE(list.GetRenderingObjects(1).empty());
    EXPECT_EQ(list.GetRenderingObjects(0).front(), &deferredTypeZero);
}
