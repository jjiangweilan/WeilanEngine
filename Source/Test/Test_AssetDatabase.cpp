#include "AssetDatabase/Asset.hpp"
#include <gtest/gtest.h>

namespace Engine
{
class IAmReferencable
{
public:
    const UUID& GetUUID() { return uuid; }
    void SetUUID(const UUID& uuid) { this->uuid = uuid; }

    UUID uuid;
};
class IAmSerializable : public Resource
{
public:
    int x;
    glm::mat4 matrix;
    IAmReferencable* referencable;

    UUID callbackTest;

    void Serialize(Serializer* s) override
    {
        s->Serialize("x", x);
        s->Serialize("matrix", matrix);
        s->Serialize("referencable", referencable);
    }

    void Deserialize(Serializer* s) override
    {
        s->Deserialize("x", x);
        s->Deserialize("matrix", matrix);
        s->Deserialize("referencable",
                       referencable,
                       [this](void* res) { callbackTest = ((IAmReferencable*)res)->GetUUID(); });
    }
};

template <>
struct AssetFactory<IAmSerializable> : public AssetRegister
{
    inline static AssetTypeID assetTypeID = GENERATE_SERIALIZABLE_FILE_ID("IAmSerializable");
    inline static char reg =
        RegisterAsset(assetTypeID, []() { return std::unique_ptr<Resource>(new IAmSerializable()); });
};

} // namespace Engine

TEST(AssetDatabase, SaveLoadFile)
{
    Engine::IAmReferencable referencable;
    auto v = std::make_unique<Engine::IAmSerializable>();
    v->x = 10;
    v->referencable = &referencable;
    v->matrix = {{1, 1, 1, 1}, {2, 2, 2, 2}, {3, 3, 3, 3}, {4, 4, 4, 4}};
    Engine::Asset newAsset(std::move(v), std::filesystem::path(TEMP_FILE_DIR) / "serializable.asset");
    Engine::JsonSerializer ser;
    newAsset.Save<Engine::IAmSerializable>(&ser);

    auto vv = (Engine::IAmSerializable*)newAsset.GetResource();
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
    readAsset.Load(&serRead);

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

    Engine::IAmSerializable* r = (Engine::IAmSerializable*)readAsset.GetResource();
    EXPECT_EQ(r->x, vv->x);
    EXPECT_EQ(r->matrix, vv->matrix);
    EXPECT_EQ(r->referencable->GetUUID(), vv->referencable->GetUUID());
    EXPECT_EQ(r->callbackTest, vv->referencable->GetUUID());
}

TEST(AssetDatabse, LoadFile) {}
