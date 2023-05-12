#include "AssetDatabase/AssetDatabase.hpp"
#include <gtest/gtest.h>

namespace Engine
{
class IAmSerializable : public Resource
{
public:
    int x;
};

template <>
struct SerializableField<IAmSerializable>
{
    static void Serialize(IAmSerializable* v, Serializer* s) { s->Serialize(v->x); }

    static void Deserialize(IAmSerializable* v, Serializer* s) { s->Deserialize(v->x); }
};

template <>
struct SerializableAsset<IAmSerializable> : public AssetRegister
{
    inline static AssetID assetID = GENERATE_SERIALIZABLE_FILE_ID("IAmSerializable");
    inline static char reg = RegisterAsset(assetID, []() { return std::unique_ptr<Resource>(new IAmSerializable()); });
};

} // namespace Engine

TEST(AssetDatabase, SaveFile)
{
    Engine::AssetDatabase adb(TEMP_FILE_DIR);

    auto v = std::make_unique<Engine::IAmSerializable>();
    adb.SaveAsset(std::move(v), "AssetDatabase/SaveFile/serializable");
}

TEST(AssetDatabse, LoadFile) {}
