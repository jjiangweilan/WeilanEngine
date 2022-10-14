#include "glbImporter.hpp"
#include "../AssetSerializer.hpp"
#include "Core/Graphics/Mesh.hpp"
#include <fstream>
namespace Engine::Internal
{
    static int MapTypeToComponentCount(const std::string& str)
    {
        if (str == "VEC4") return 4;
        else if (str == "VEC3") return 3;
        else if (str == "VEC2") return 2;
        else if (str == "MAT2") return 4;
        else if (str == "MAT3") return 9;
        else if (str == "MAT4") return 16;
        else if (str == "SCALAR") return 1;
        assert(0);
        return 0;
    }

    static int MapComponentTypeBitSize(const uint32_t& componentType)
    {
        if (componentType == 5120 || componentType == 5121) return 8;
        else if (componentType == 5122 || componentType == 5123) return 16;
        else if (componentType == 5125 || componentType == 5126) return 32;
        assert(0);
        return 0;
    }

    struct glbImportHelper
    {
        nlohmann::json j;
        nlohmann::json accessors;
        nlohmann::json bufferViews;
        RefPtr<const std::unordered_map<std::string, UUID>> preloadedUUIDs;

        uint32_t* bufChunkData;
        void ProcessMesh(int mesh);
        void Load(const std::filesystem::path& path);

        template<class T>
        void AddDataToVertexDescription(uint32_t accessorIndex, VertexAttribute<T>& attr, uint32_t typeCheck = 0);
        void AddDataToVertexDescription(uint32_t accessorIndex, UntypedVertexAttribute& attr, uint32_t typeCheck = 0);

        std::unordered_map<std::string, UniPtr<Mesh>> meshes;
    };


    UniPtr<AssetObject> glbImporter::Import(const std::filesystem::path& path, ReferenceResolver& refResolver, const UUID& uuid, const std::unordered_map<std::string, UUID>& containedUUIDs)
    {
        std::ifstream input(path, std::ios::binary | std::ios::in);
        if (!input.is_open() || !input.good()) return nullptr;

        glbImportHelper importHelper;
        importHelper.preloadedUUIDs = &containedUUIDs;
        importHelper.Load(path);

        auto model = MakeUnique<Model>(std::move(importHelper.meshes), uuid);
        model->SetName(path.filename().string());
        return model;
    }

    template<class T>
    void glbImportHelper::AddDataToVertexDescription(uint32_t accessorIndex, VertexAttribute<T>& attr, uint32_t typeCheck)
    {
        auto& acce = accessors[accessorIndex];

        assert(typeCheck == 0 || acce["componentType"] == typeCheck);

        auto& bufView = bufferViews[acce["bufferView"].get<int>()];
        attr.count = acce["count"];
        char* startAddr = ((char*)bufChunkData + bufView["byteOffset"].get<int>());
        attr.componentCount = MapTypeToComponentCount(acce["type"]);
        attr.data = std::vector<unsigned char>(startAddr, startAddr + attr.count * (uint32_t)attr.componentCount * sizeof(T));
    }

    void glbImportHelper::AddDataToVertexDescription(uint32_t accessorIndex, UntypedVertexAttribute& attr, uint32_t typeCheck)
    {
        auto& acce = accessors[accessorIndex];

        assert(typeCheck == 0 || acce["componentType"] == typeCheck);

        auto& bufView = bufferViews[acce["bufferView"].get<int>()];
        attr.count = acce["count"];
        char* startAddr = (char*)bufChunkData + bufView["byteOffset"].get<int>();
        attr.componentCount = MapTypeToComponentCount(acce["type"]);
        const int ByteToBit = 8;
        attr.dataByteSize = MapComponentTypeBitSize(acce["componentType"]) / ByteToBit;
        attr.data = std::vector<unsigned char>(startAddr, startAddr + attr.count * attr.componentCount * attr.dataByteSize);
    }

    void glbImportHelper::Load(const std::filesystem::path& path)
    {
        uint32_t magic;
        uint32_t version;
        uint32_t length;

        std::ifstream gltf;
        gltf.open(path, std::ios_base::binary);
        uint32_t header[3];
        gltf.read((char*)header, 12);
        magic = header[0];
        version = header[1];
        length = header[2];

        assert(magic == 0x46546C67);
        assert(version == 2);

        std::vector<uint32_t> pData(length / sizeof(uint32_t));
        gltf.seekg(std::ios_base::beg);
        gltf.read((char*)pData.data(), length);

        // load json
        uint32_t jsonChunkLength = pData[3];
        uint32_t jsonChunkType = pData[4];
        assert(jsonChunkType == 0x4E4F534A);
        char* jsonChunkData = (char*)(pData.data() + 5);

        // remove chunk padding
        for(uint32_t i = jsonChunkLength - 1; i >= 0; --i)
        {
            if (jsonChunkData[i] != ' ')
            {
                jsonChunkData[i + 1] = '\0';
                break;
            }
        }

        uint32_t bufOffset = 5 + jsonChunkLength / sizeof(uint32_t);
        uint32_t bufChunkType = pData[bufOffset + 1];
        assert(bufChunkType == 0x004E4942);
        bufChunkData = pData.data() + bufOffset + 2;

        j = nlohmann::json::parse(jsonChunkData);
        assert(j["buffer"].contains("uri") == false && "we only process valid .glb files, which shouldn't have a defined uri according to specs");

        accessors = j["accessors"];
        bufferViews = j["bufferViews"];

        // process meshes
        nlohmann::json& meshesInfo = j["meshes"];
        uint32_t meshCount = meshesInfo.size();
        for(uint32_t i = 0; i < meshCount ; ++i)
        {
            ProcessMesh(i);
        }
    }

    void glbImportHelper::ProcessMesh(int meshIndex)
    {
        auto&meshJson = j["meshes"][meshIndex];

        // feat: currently we don't have a concept like submesh, so one mesh can only contains one primitive(submesh)
        assert(meshJson["primitives"].size() < 2);

        std::string meshName = meshJson["name"];
        for(auto& prim : meshJson["primitives"])
        {
            VertexDescription vertDesc;
            auto& attrs = prim["attributes"];

            AddDataToVertexDescription(prim["indices"], vertDesc.index);
            AddDataToVertexDescription(attrs["POSITION"], vertDesc.position, 5126);
            AddDataToVertexDescription(attrs["NORMAL"], vertDesc.normal, 5126);
            if (attrs.contains("COLOR_0"))
            {
                vertDesc.colors.emplace_back();
                AddDataToVertexDescription(attrs["COLOR_0"], vertDesc.colors[0]);
            }

            UUID uuid = UUID::empty;
            auto uuidIter = preloadedUUIDs->find(meshName);
            if (uuidIter != preloadedUUIDs->end())
            {
                uuid = uuidIter->second;
            }

            UniPtr<Mesh> mesh = MakeUnique<Mesh>(std::move(vertDesc), meshName, uuid);

            meshes[meshName] = std::move(mesh);
        }
    }
}
