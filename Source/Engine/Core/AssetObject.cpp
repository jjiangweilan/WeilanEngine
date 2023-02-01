#include "AssetObject.hpp"
#include <cassert>

#include <spdlog/spdlog.h>
#include <sstream>
namespace Engine
{
AssetObject::AssetObject(const UUID& uuid)
{
    if (uuid.IsEmpty())
    {
        this->uuid = UUID();
    }
    else
    {
        this->uuid = uuid;
    }
}

void AssetObject::Reload(AssetObject&& loaded)
{
    this->name = std::move(loaded.name);
    this->uuid = std::exchange(loaded.uuid, UUID::empty);
    this->editableMembers = std::move(loaded.editableMembers);
    this->assetMembers = std::move(loaded.assetMembers);
}

const UUID& AssetObject::GetUUID() const { return uuid; }

unsigned char* AssetFileDataWrite(const float& val, size_t& size)
{
    size = sizeof(val);
    unsigned char* ptr = new unsigned char[size];
    memcpy(ptr, &val, size);
    return ptr;
}

bool AssetObject::Serialize(AssetSerializer& serializer) { return SerializeInternal("_root_", serializer); }

bool AssetObject::SerializeInternal(const std::string& nameChain, AssetSerializer& serializer)
{
    SerializeSelf("_root_", serializer);
    SerializePrivate("_root_", serializer);
    return true;
}

void AssetObject::Deserialize(AssetSerializer& serializer, ReferenceResolver& refResolver)
{
    DeserializeInternal("_root_", serializer, refResolver);
}

void AssetObject::DeserializeInternal(const std::string& nameChain,
                                      AssetSerializer& serializer,
                                      ReferenceResolver& refResolver)
{
    DeserializeSelf(nameChain, serializer);
    DeserializePrivate(nameChain, serializer, refResolver);
}

void AssetObject::SerializePrivate(const std::string& nameChain, AssetSerializer& serializer)
{
    for (auto& iter : editableMembers)
    {
        std::ostringstream oss;
        oss << nameChain << "." << iter.second.editable->GetName();
        std::string nextNameChain = oss.str();

        iter.second.editable->Serialize(nextNameChain, serializer);
    }
}

void AssetObject::DeserializePrivate(const std::string& nameChain,
                                     AssetSerializer& serializer,
                                     ReferenceResolver& refResolver)
{
    for (auto& iter : editableMembers)
    {
        std::ostringstream oss;
        oss << nameChain << "." << iter.second.editable->GetName();
        std::string nextNameChain = oss.str();

        iter.second.editable->Deserialize(nextNameChain, serializer, refResolver);
    }
}

void AssetObject::SerializeSelf(const std::string& nameChain, AssetSerializer& serializer)
{
    // write name
    std::ostringstream nameMemberNameChain;
    nameMemberNameChain << nameChain << "._name_";
    serializer.Write(nameMemberNameChain.str(), GetName());

    // wrtie UUID
    std::ostringstream uuidMemberNameChain;
    uuidMemberNameChain << nameChain << "._uuid_";
    serializer.Write(uuidMemberNameChain.str(), GetUUID());
}

void AssetObject::DeserializeSelf(const std::string& nameChain, AssetSerializer& serializer)
{
    // read name
    std::ostringstream nameMemberNameChain;
    nameMemberNameChain << nameChain << "._name_";
    std::string serName;
    serializer.Read(nameMemberNameChain.str(), serName);
    SetName(serName);

    // read UUID
    std::ostringstream uuidMemberNameChain;
    uuidMemberNameChain << nameChain << "._uuid_";
    UUID serUUID;
    serializer.Read(uuidMemberNameChain.str(), serUUID);
    uuid = serUUID;
}

std::vector<RefPtr<AssetObject>> AssetObject::GetAssetObjectMembers() { return assetMembers; }
} // namespace Engine
