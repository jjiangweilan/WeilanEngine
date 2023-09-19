#include "Asset/Material.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Asset.hpp"
#include "Libs/Serialization/JsonSerializer.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace Engine
{
class IAmReferencable
{
public:
    const UUID& GetUUID()
    {
        return uuid;
    }
    void SetUUID(const UUID& uuid)
    {
        this->uuid = uuid;
    }

    UUID uuid;
};

class IAmSerializable : public Serializable
{
public:
    int x;

    void Serialize(Serializer* s) const override
    {
        s->Serialize("x", x);
    }

    void Deserialize(Serializer* s) override
    {
        s->Deserialize("x", x);
    }
};

class IAmResource : public Asset
{
    DECLARE_ASSET();

public:
    int x;
    glm::mat4 matrix;
    IAmResource* referencable;
    std::vector<IAmSerializable> serializableArray;
    std::unordered_map<std::string, int> unorderedMap;

    UUID callbackTest;

    void Serialize(Serializer* s) const override
    {
        Asset::Serialize(s);
        s->Serialize("x", x);
        s->Serialize("matrix", matrix);
        s->Serialize("referencable", referencable);
        s->Serialize("serializableArray", serializableArray);
        s->Serialize("unorderedMap", unorderedMap);
    }

    void Deserialize(Serializer* s) override
    {
        Asset::Deserialize(s);
        s->Deserialize("x", x);
        s->Deserialize("matrix", matrix);
        s->Deserialize(
            "referencable",
            referencable,
            [this](void* res) { callbackTest = ((IAmReferencable*)res)->GetUUID(); }
        );
        s->Deserialize("serializableArray", serializableArray);
        s->Deserialize("unorderedMap", unorderedMap);
    }
};
DEFINE_ASSET(IAmResource, "6BD39F47-4980-4134-A0C3-E5BCBDC1A92F", "resExtTest");

} // namespace Engine

using namespace Engine;
TEST(AssetDatabase, Serialization)
{
    AssetDatabase db0("./");

    std::unique_ptr<Engine::IAmResource> referencable = std::make_unique<IAmResource>();
    std::unique_ptr<IAmResource> vv = std::make_unique<IAmResource>();
    vv->x = 10;
    vv->referencable = referencable.get();
    vv->matrix = {{1, 1, 1, 1}, {2, 2, 2, 2}, {3, 3, 3, 3}, {4, 4, 4, 4}};
    vv->serializableArray.emplace_back();
    vv->serializableArray.emplace_back();
    vv->serializableArray[0].x = 10;
    vv->serializableArray[1].x = 100;
    vv->unorderedMap["123"] = 123;
    vv->unorderedMap["321"] = 321;

    // create a new asset
    IAmResource* v = (IAmResource*)db0.SaveAsset(std::move(vv), "testRes");
    db0.SaveAsset(std::move(referencable), "testRef");

    // This doesn't really test loading an asset from disk because it's already cached in the database
    auto r = (IAmResource*)db0.LoadAsset("testRes.resExtTest");
    auto ref = (IAmResource*)db0.LoadAsset("testRef.resExtTest");

    EXPECT_EQ(r->x, v->x);
    EXPECT_EQ(r->matrix, v->matrix);
    EXPECT_EQ(r->referencable->GetUUID(), ref->GetUUID());
    EXPECT_EQ(r->serializableArray[0].x, v->serializableArray[0].x);
    EXPECT_EQ(r->serializableArray[1].x, v->serializableArray[1].x);
    EXPECT_EQ(r->unorderedMap, v->unorderedMap);
}

TEST(AssetDatabase, Material) {}

TEST(AssetDatabse, LoadFile) {}
