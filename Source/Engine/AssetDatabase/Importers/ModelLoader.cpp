#include "ModelLoader.hpp"
#include "Core/Model.hpp"
#include <assimp/Importer.hpp>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
DEFINE_ASSET_LOADER(ModelLoader, "glb")

struct ImporterImple
{
    std::vector<std::unique_ptr<Submesh>> submeshes;
    std::vector<std::unique_ptr<Texture>> textures;
    std::vector<std::unique_ptr<Material>> materials;

    void Load(const std::filesystem::path& path)
    {
        Assimp::Importer importer;
        scene = importer.ReadFile(path.string().c_str(), aiProcess_JoinIdenticalVertices | aiProcess_Triangulate);

        if (scene == nullptr)
        {
            spdlog::error(importer.GetErrorString());
            return;
        }

        ProcessMesh();
        ProcessMaterial();
    }

    void ProcessMesh()
    {
        for (int meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++)
        {
            aiMesh* mesh = scene->mMeshes[meshIndex];

            std::vector<glm::vec3> positions(mesh->mNumVertices);
            for (int i = 0; i < mesh->mNumVertices; ++i)
            {
                auto& v = mesh->mVertices[i];
                positions[i].x = v.x;
                positions[i].y = v.y;
                positions[i].z = v.z;
            }

            VertexAttributes attributes;
            uint32_t attributeStrideSize = 0;
            const uint32_t normalSize = 12;
            const uint32_t tangentSize = 16;
            uint32_t normalStrideOffset = 0;
            uint32_t tangentStrideOffset = 0;
            uint32_t texCoordStrideOffsets[8];
            uint32_t vertexColorStrideOffsets[8];
            if (mesh->HasNormals())
            {
                attributeStrideSize += normalSize ? normalSize : 0;
                attributes.AddAttribute("normal", normalSize);
            }

            if (mesh->HasTangentsAndBitangents())
            {
                tangentStrideOffset = attributeStrideSize;
                attributeStrideSize += tangentSize; // we only use tangent
                attributes.AddAttribute("tangent", tangentSize);
            }

            const char* texCoordNames[8] = {
                "texCoords_0",
                "texCoords_1",
                "texCoords_2",
                "texCoords_3",
                "texCoords_4",
                "texCoords_5",
                "texCoords_6",
                "texCoords_7"
            };
            for (int i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++i)
            {
                if (mesh->HasTextureCoords(i))
                {
                    texCoordStrideOffsets[i] = attributeStrideSize;
                    uint32_t size = mesh->mNumUVComponents[i] * 4;
                    attributeStrideSize += size;
                    attributes.AddAttribute(texCoordNames[i], size);
                }
            }

            const char* vertexColorNames[8] = {
                "vertexColor_0",
                "vertexColor_1",
                "vertexColor_2",
                "vertexColor_3",
                "vertexColor_4",
                "vertexColor_5",
                "vertexColor_6",
                "vertexColor_7"
            };
            const uint32_t vertexColorSize = 16;
            for (int i = 0; i < AI_MAX_NUMBER_OF_COLOR_SETS; ++i)
            {
                if (mesh->HasVertexColors(i))
                {
                    vertexColorStrideOffsets[i] = attributeStrideSize;
                    attributeStrideSize += vertexColorSize;
                    attributes.AddAttribute(vertexColorNames[i], vertexColorSize);
                }
            }

            std::vector<uint8_t> attributeData(attributeStrideSize * mesh->mNumVertices);

            uint32_t strideOffset = 0;
            uint8_t* data = attributeData.data();
            if (mesh->HasNormals())
            {
                for (int i = 0; i < mesh->mNumVertices; ++i)
                {
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + normalStrideOffset) =
                        mesh->mNormals[0].x;
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + normalStrideOffset + 4) =
                        mesh->mNormals[1].y;
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + normalStrideOffset + 8) =
                        mesh->mNormals[2].z;
                }
            }

            if (mesh->HasTangentsAndBitangents())
            {
                for (int i = 0; i < mesh->mNumVertices; ++i)
                {
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset) =
                        mesh->mTangents[0].x;
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset + 4) =
                        mesh->mTangents[1].y;
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset + 8) =
                        mesh->mTangents[2].z;
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset + 12) =
                        mesh->mTangents[3].z;
                }
            }

            for (int i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++i)
            {
                if (mesh->HasTextureCoords(i))
                {
                    for (int vi = 0; vi < mesh->mNumVertices; ++vi)
                    {
                        for (int uvi = 0; uvi < mesh->mNumUVComponents[i]; uvi++)
                        {
                            *reinterpret_cast<float*>(
                                data + attributeStrideSize * i + texCoordStrideOffsets[i] + uvi * 4
                            ) = mesh->mTextureCoords[i][vi].x;
                        }
                    }
                }
            }

            attributes.SetData(std::move(attributeData));

            std::unique_ptr<Submesh> submesh = std::make_unique<Submesh>();
            submesh->SetPositions(std::move(positions));
            submesh->SetVertexAttribute(std::move(attributes));
            submeshes.push_back(std::move(submesh));
        }
    }

    void ProcessMaterial()
    {
        for (int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
        {
            std::unique_ptr<Material> mat = std::make_unique<Material>();
            auto material = scene->mMaterials[materialIndex];

            aiColor4D baseColorFactor = {0.5, 0.5, 0.5, 0.5};
            aiColor4D emissive = {0, 0, 0, 0};
            float roughness = 0.4f;
            float metallic = 0.2f;
            float alphaCutoff = 0.5f;
            material->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, baseColorFactor);
            material->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissive);
            material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
            material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
            material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff);
            aiTexture *diffuseTex, metallicRoughnessTex, normalTex, emissiveTex;
            material->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), diffuseTex);
            material->Get(AI_MATKEY_TEXTURE(aiTextureType_NORMALS, 0), normalTex);
            material->Get(AI_MATKEY_TEXTURE(aiTextureType_METALNESS, 0), metallicRoughnessTex);
            material->Get(AI_MATKEY_TEXTURE(aiTextureType_EMISSIVE, 0), emissiveTex);

            mat->SetVector(
                "PBR",
                "baseColorFactor",
                {baseColorFactor.r, baseColorFactor.g, baseColorFactor.b, baseColorFactor.a}
            );
            mat->SetVector("PBR", "emissive", {emissive.r, emissive.g, emissive.b, emissive.a});
            mat->SetFloat("PBR", "roughness", roughness);
            mat->SetFloat("PBR", "metallic", metallic);
            mat->SetFloat("PBR", "alphaCutoff", alphaCutoff);
            materials.push_back(std::move(mat));
        }
    }

private:
    const aiScene* scene;
};

const std::vector<std::type_index>& ModelLoader::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Model)};
    return types;
}

void ModelLoader::Load()
{
    asset = std::make_unique<Model>();
    asset->LoadFromFile(absoluteAssetPath.string().c_str());
}
