#include "AssetDatabase/Asset.hpp"
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

class IAmResource : public Resource
{
public:
    int x;
    glm::mat4 matrix;
    IAmReferencable* referencable;
    std::vector<IAmSerializable> serializableArray;

    UUID callbackTest;

    void Serialize(Serializer* s) const override
    {
        s->Serialize("x", x);
        s->Serialize("matrix", matrix);
        s->Serialize("referencable", referencable);
        s->Serialize("serializableArray", serializableArray);
    }

    void Deserialize(Serializer* s) override
    {
        s->Deserialize("x", x);
        s->Deserialize("matrix", matrix);
        s->Deserialize(
            "referencable",
            referencable,
            [this](void* res) { callbackTest = ((IAmReferencable*)res)->GetUUID(); }
        );
        s->Deserialize("serializableArray", serializableArray);
    }
};

template <>
struct AssetFactory<IAmResource> : public ResourceRegistry
{
    inline static ResourceTYpeID assetTypeID = GENERATE_SERIALIZABLE_FILE_ID("IAmSerializable");
    inline static char reg = RegisterAsset(assetTypeID, []() { return std::unique_ptr<Resource>(new IAmResource()); });
};

} // namespace Engine

TEST(AssetDatabase, SaveLoadFile)
{
    Engine::IAmReferencable referencable;
    auto v = std::make_unique<Engine::IAmResource>();
    v->x = 10;
    v->referencable = &referencable;
    v->matrix = {{1, 1, 1, 1}, {2, 2, 2, 2}, {3, 3, 3, 3}, {4, 4, 4, 4}};
    v->serializableArray.emplace_back();
    v->serializableArray.emplace_back();
    v->serializableArray[0].x = 10;
    v->serializableArray[1].x = 100;

    // create a new asset
    Engine::Asset newAsset(std::move(v), std::filesystem::path(TEMP_FILE_DIR) / "serializable.asset");
    Engine::JsonSerializer ser;
    newAsset.Serialize<Engine::IAmResource>(&ser);

    // write serialized file to disk
    std::ofstream out;
    out.open(newAsset.GetPath(), std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if (out.is_open() && out.good())
    {
        auto binary = ser.GetBinary();

        if (binary.size() != 0)
        {
            out.write((char*)binary.data(), binary.size());
        }
    }
    out.close();

    auto vv = (Engine::IAmResource*)newAsset.GetResource();
    Engine::Asset readAsset(std::filesystem::path(TEMP_FILE_DIR) / "serializable.asset");

    std::ifstream in;
    in.open(newAsset.GetPath(), std::ios_base::in | std::ios_base::binary);
    nlohmann::json j;
    if (in.is_open() && in.good())
    {
        size_t size = std::filesystem::file_size(newAsset.GetPath());
        if (size != 0)
        {
            std::string json;
            in >> json;
            j = nlohmann::json::parse(json);
        }
    }
    in.close();

    Engine::SerializeReferenceResolveMap referenceResolve;
    Engine::JsonSerializer serRead(j, &referenceResolve);
    readAsset.Deserialize(&serRead);

    for (auto& iter : referenceResolve)
    {
        if (iter.first == referencable.GetUUID())
        {
            for (auto& resolve : iter.second)
            {
                resolve.target = &referencable;
                resolve.callback(&referencable);
            }
        }
    }

    Engine::IAmResource* r = (Engine::IAmResource*)readAsset.GetResource();
    EXPECT_EQ(r->x, vv->x);
    EXPECT_EQ(r->matrix, vv->matrix);
    EXPECT_EQ(r->referencable->GetUUID(), vv->referencable->GetUUID());
    EXPECT_EQ(r->serializableArray[0].x, vv->serializableArray[0].x);
    EXPECT_EQ(r->serializableArray[1].x, vv->serializableArray[1].x);
}

TEST(AssetDatabse, LoadFile) {}
