#include "ModelImporter.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Runtime/Object/Component/AnimationPlayer.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/Component/PhysicsBody.hpp"
#include "Engine/Runtime/Object/Mesh/Model.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Engine/Runtime/System/AssetDatabase/ArtifactTypes.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/AssetDatabase/Exporters/KtxExporter.hpp"
#include "Engine/Runtime/System/AssetDatabase/ModelArtifact.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "Engine/ThirdParty/stb/stb_image.h"
#include "Engine/ThirdParty/xxHash/xxhash.h"
#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cstring>
#include <fstream>
#include <meshoptimizer.h>
#include <set>

DEFINE_ASSET_IMPORTER(ModelImporter, "glb,gltf,fbx");

namespace
{
constexpr uint64_t ModelImporterVersion = 6;

uint64_t ComputeMetaHash(const nlohmann::json& meta)
{
    return std::hash<std::string>{}(meta.value("importOption", nlohmann::json::object()).dump()) ^ ModelImporterVersion;
}

uint64_t ComputeContentHash(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.good())
    {
        return 0;
    }

    const auto fileSize = std::filesystem::file_size(path);
    std::vector<char> fileData(fileSize);
    f.read(fileData.data(), static_cast<std::streamsize>(fileSize));
    return XXH3_64bits(fileData.data(), fileData.size());
}

glm::mat4 AiMatrixToGlm(aiMatrix4x4 m)
{
    m = m.Transpose();
    return {
        m.a1, m.a2, m.a3, m.a4,
        m.b1, m.b2, m.b3, m.b4,
        m.c1, m.c2, m.c3, m.c4,
        m.d1, m.d2, m.d3, m.d4,
    };
}

void ApplyTransform(GameObject& gameObject, const glm::mat4& transform)
{
    glm::vec3 position;
    glm::vec3 scale;
    glm::quat rotation;
    Math::DecomposeMatrix(transform, position, scale, rotation);
    gameObject.SetLocalPosition(position);
    gameObject.SetLocalScale(scale);
    gameObject.SetLocalRotation(rotation);
}

uint32_t GetSamplerIndex(aiMaterial* material, aiTextureType type, const char* bindingName, const aiString& texName)
{
    int wrapU = aiTextureMapMode_Wrap;
    int wrapV = aiTextureMapMode_Wrap;
    material->Get(AI_MATKEY_MAPPINGMODE_U(type, 0), wrapU);
    material->Get(AI_MATKEY_MAPPINGMODE_V(type, 0), wrapV);
    if (wrapV != wrapU)
    {
        spdlog::warn(
            "ModelImporter: texture '{}' binding '{}' has different U ({}) and V ({}) wrap modes; using U mode.",
            texName.C_Str(),
            bindingName,
            wrapU,
            wrapV
        );
    }

    int filterMin = 9729;
    material->Get(AI_MATKEY_GLTF_MAPPINGFILTER_MIN(type, 0), filterMin);

    uint32_t addrIdx = 0;
    switch (wrapU)
    {
        case aiTextureMapMode_Wrap: addrIdx = 0; break;
        case aiTextureMapMode_Mirror: addrIdx = 1; break;
        case aiTextureMapMode_Clamp: addrIdx = 2; break;
        case aiTextureMapMode_Decal: addrIdx = 3; break;
        default: addrIdx = 0; break;
    }

    uint32_t filterIdx = 1;
    switch (filterMin)
    {
        case 9728:
        case 9984:
        case 9986: filterIdx = 0; break;
        default: filterIdx = 1; break;
    }

    return addrIdx * 2 + filterIdx;
}

void OptimizeMesh(std::vector<glm::vec3>& positions, std::vector<uint8_t>& attributeData, uint32_t attributeStride, std::vector<uint32_t>& indices)
{
    if (positions.empty() || indices.empty())
    {
        return;
    }

    const size_t positionStride = sizeof(glm::vec3);
    const size_t vertexStride = positionStride + attributeStride;
    std::vector<uint8_t> vertexData(vertexStride * positions.size());
    for (size_t i = 0; i < positions.size(); ++i)
    {
        std::memcpy(vertexData.data() + vertexStride * i, &positions[i], positionStride);
        if (attributeStride > 0)
        {
            std::memcpy(
                vertexData.data() + vertexStride * i + positionStride,
                attributeData.data() + attributeStride * i,
                attributeStride
            );
        }
    }

    std::vector<unsigned int> remap(positions.size());
    size_t vertexCount = meshopt_generateVertexRemap(
        remap.data(), indices.data(), indices.size(), vertexData.data(), positions.size(), vertexStride
    );

    std::vector<uint32_t> remappedIndices(indices.size());
    std::vector<glm::vec3> remappedPositions(vertexCount);
    std::vector<uint8_t> remappedAttributes(attributeStride * vertexCount);
    meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(), remap.data());
    meshopt_remapVertexBuffer(remappedPositions.data(), positions.data(), positions.size(), sizeof(glm::vec3), remap.data());
    if (attributeStride > 0)
    {
        meshopt_remapVertexBuffer(remappedAttributes.data(), attributeData.data(), positions.size(), attributeStride, remap.data());
    }

    meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), remappedIndices.size(), vertexCount);
    meshopt_optimizeOverdraw(
        remappedIndices.data(),
        remappedIndices.data(),
        remappedIndices.size(),
        &remappedPositions[0].x,
        vertexCount,
        sizeof(glm::vec3),
        1.05f
    );

    std::vector<unsigned int> fetchRemap(vertexCount);
    size_t fetchedVertexCount = meshopt_optimizeVertexFetchRemap(
        fetchRemap.data(), remappedIndices.data(), remappedIndices.size(), vertexCount
    );
    std::vector<uint32_t> fetchedIndices(remappedIndices.size());
    std::vector<glm::vec3> fetchedPositions(fetchedVertexCount);
    std::vector<uint8_t> fetchedAttributes(attributeStride * fetchedVertexCount);
    meshopt_remapIndexBuffer(fetchedIndices.data(), remappedIndices.data(), remappedIndices.size(), fetchRemap.data());
    meshopt_remapVertexBuffer(
        fetchedPositions.data(), remappedPositions.data(), vertexCount, sizeof(glm::vec3), fetchRemap.data()
    );
    if (attributeStride > 0)
    {
        meshopt_remapVertexBuffer(
            fetchedAttributes.data(), remappedAttributes.data(), vertexCount, attributeStride, fetchRemap.data()
        );
    }

    positions = std::move(fetchedPositions);
    attributeData = std::move(fetchedAttributes);
    indices = std::move(fetchedIndices);
}

struct ModelImportContext
{
    const aiScene* scene = nullptr;
    const std::filesystem::path* absoluteAssetPath = nullptr;
    const ImportDatabase* importDatabase = nullptr;
    const std::unordered_map<std::string, UUID>* internalNameToUUID = nullptr;
    UUID sourceAssetUUID;

    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::unique_ptr<Texture>> embeddedTextures;
    std::vector<std::unique_ptr<Material>> materials;
    std::vector<std::unique_ptr<Animation>> animations;
    std::vector<std::unique_ptr<GameObject>> gameObjects;
    std::vector<ObjPtr<GameObject>> roots;
    std::vector<ImportDatabase::ArtifactRecord> artifacts;
    std::unordered_map<int, Texture*> embeddedTextureByIndex;

    UUID GetSubAssetUUID(std::string_view name, std::string_view typeName, AssetArtifacts::Kind kind, std::string_view locator) const
    {
        std::string key = fmt::format("{}-{}", name, typeName);
        if (internalNameToUUID != nullptr)
        {
            auto iter = internalNameToUUID->find(key);
            if (iter != internalNameToUUID->end() && !iter->second.IsEmpty())
            {
                return iter->second;
            }
        }

        return AssetArtifacts::MakeArtifactUUID(sourceAssetUUID, kind, locator);
    }

    Texture* ImportEmbeddedTexture(int textureIndex)
    {
        auto iter = embeddedTextureByIndex.find(textureIndex);
        if (iter != embeddedTextureByIndex.end())
        {
            return iter->second;
        }

        if (textureIndex < 0 || textureIndex >= static_cast<int>(scene->mNumTextures))
        {
            return nullptr;
        }

        aiTexture* texture = scene->mTextures[textureIndex];
        int width = 0;
        int height = 0;
        int channels = 4;
        std::unique_ptr<uint8_t, void (*)(void*)> decoded(nullptr, [](void* p) { stbi_image_free(p); });
        std::vector<uint8_t> rawRgba;

        if (texture->mHeight == 0)
        {
            decoded.reset(stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(texture->pcData),
                static_cast<int>(texture->mWidth),
                &width,
                &height,
                &channels,
                4
            ));
            if (!decoded)
            {
                return nullptr;
            }
        }
        else
        {
            width = static_cast<int>(texture->mWidth);
            height = static_cast<int>(texture->mHeight);
            rawRgba.resize(static_cast<size_t>(width) * height * 4);
            for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i)
            {
                rawRgba[i * 4 + 0] = texture->pcData[i].r;
                rawRgba[i * 4 + 1] = texture->pcData[i].g;
                rawRgba[i * 4 + 2] = texture->pcData[i].b;
                rawRgba[i * 4 + 3] = texture->pcData[i].a;
            }
        }

        std::string textureName = fmt::format("texture_{}", textureIndex);
        UUID artifactUUID = GetSubAssetUUID(textureName, Texture::StaticGetTypeName(), AssetArtifacts::Kind::Texture, fmt::format("{}", textureIndex));
        auto relativePath = AssetArtifacts::MakeArtifactPath(artifactUUID, AssetArtifacts::Kind::Texture);
        auto absolutePath = importDatabase->GetImportDatabaseRootPath() / relativePath;
        Exporters::KtxExporter::Export(
            absolutePath.string().c_str(),
            decoded ? decoded.get() : rawRgba.data(),
            width,
            height,
            1,
            2,
            1,
            1,
            false,
            false,
            Gfx::GfxFormat::R8G8B8A8_SRGB,
            true
        );

        auto textureAsset = std::make_unique<Texture>();
        textureAsset->SetUUID(artifactUUID);
        textureAsset->SetName(textureName);
        Texture* texturePtr = textureAsset.get();
        embeddedTextures.push_back(std::move(textureAsset));
        embeddedTextureByIndex[textureIndex] = texturePtr;
        artifacts.push_back({artifactUUID, sourceAssetUUID, std::string(AssetArtifacts::ToString(AssetArtifacts::Kind::Texture)), textureName, relativePath, false, fmt::format("texture/{}", textureIndex)});
        return texturePtr;
    }

    Texture* TryImportEmbeddedTexture(const aiString& textureName)
    {
        if (textureName.length == 0 || textureName.C_Str()[0] != '*')
        {
            return nullptr;
        }

        int textureIndex = std::atoi(textureName.C_Str() + 1);
        return ImportEmbeddedTexture(textureIndex);
    }
};

void ProcessMeshes(ModelImportContext& context)
{
    for (int meshIndex = 0; meshIndex < static_cast<int>(context.scene->mNumMeshes); meshIndex++)
    {
        aiMesh* mesh = context.scene->mMeshes[meshIndex];

        std::vector<glm::vec3> positions(mesh->mNumVertices);
        for (int i = 0; i < static_cast<int>(mesh->mNumVertices); ++i)
        {
            positions[i] = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
        }

        VertexAttributes attributes;
        uint32_t attributeStrideSize = 0;
        const uint32_t normalSize = 12;
        const uint32_t tangentSize = 16;
        uint32_t normalStrideOffset = 0;
        uint32_t tangentStrideOffset = 0;
        uint32_t texCoordStrideOffsets[8]{};
        uint32_t vertexColorStrideOffsets[8]{};
        uint32_t skeletonOffset = 0;

        if (mesh->HasNormals())
        {
            attributeStrideSize += normalSize;
            attributes.AddAttribute("NORMAL0", VertexAttributeSemantics::Normal, 0, normalSize);
        }
        if (mesh->HasTangentsAndBitangents())
        {
            tangentStrideOffset = attributeStrideSize;
            attributeStrideSize += tangentSize;
            attributes.AddAttribute("TANGENT0", VertexAttributeSemantics::Tangent, 0, tangentSize);
        }

        const char* texCoordNames[8] = {"TEXCOORD0", "TEXCOORD1", "TEXCOORD2", "TEXCOORD3", "TEXCOORD4", "TEXCOORD5", "TEXCOORD6", "TEXCOORD7"};
        const int MaxTexcoordChannels = 8;
        for (int i = 0; i < MaxTexcoordChannels; ++i)
        {
            if (mesh->HasTextureCoords(i))
            {
                texCoordStrideOffsets[i] = attributeStrideSize;
                uint32_t size = mesh->mNumUVComponents[i] * 4;
                attributeStrideSize += size;
                attributes.AddAttribute(texCoordNames[i], VertexAttributeSemantics::Texcoord, i, size);
            }
        }

        const char* vertexColorNames[8] = {"COLOR0", "COLOR1", "COLOR2", "COLOR3", "COLOR4", "COLOR5", "COLOR6", "COLOR7"};
        const uint32_t vertexColorSize = 16; // assimp imported vertex color size is always 4 channels no matter if it's color2 or color3 or color4
        for (int i = 0; i < AI_MAX_NUMBER_OF_COLOR_SETS; ++i)
        {
            if (mesh->HasVertexColors(i))
            {
                vertexColorStrideOffsets[i] = attributeStrideSize;
                attributeStrideSize += vertexColorSize;
                attributes.AddAttribute(vertexColorNames[i], VertexAttributeSemantics::Color, i, vertexColorSize);
            }
        }

        const uint32_t boneSize = 16;
        if (mesh->HasBones())
        {
            skeletonOffset = attributeStrideSize;
            attributeStrideSize += boneSize;
            attributes.AddAttribute("BONE0", VertexAttributeSemantics::Bone, 0, boneSize);
        }

        std::vector<uint8_t> attributeData(attributeStrideSize * mesh->mNumVertices, 0);
        uint8_t* data = attributeData.data();

        if (mesh->HasNormals())
        {
            for (int i = 0; i < static_cast<int>(mesh->mNumVertices); ++i)
            {
                *reinterpret_cast<float*>(data + attributeStrideSize * i + normalStrideOffset) = mesh->mNormals[i].x;
                *reinterpret_cast<float*>(data + attributeStrideSize * i + normalStrideOffset + 4) = mesh->mNormals[i].y;
                *reinterpret_cast<float*>(data + attributeStrideSize * i + normalStrideOffset + 8) = mesh->mNormals[i].z;
            }
        }

        if (mesh->HasTangentsAndBitangents())
        {
            for (int i = 0; i < static_cast<int>(mesh->mNumVertices); ++i)
            {
                glm::vec3 tangent = {mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z};
                glm::vec3 bitangent = {mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z};
                glm::vec3 normal = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
                float w = glm::sign(dot(glm::cross(tangent, bitangent), normal));
                *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset) = mesh->mTangents[i].x;
                *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset + 4) = mesh->mTangents[i].y;
                *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset + 8) = mesh->mTangents[i].z;
                *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset + 12) = w;
            }
        }

        for (int i = 0; i < MaxTexcoordChannels; ++i)
        {
            if (mesh->HasTextureCoords(i))
            {
                for (int vi = 0; vi < static_cast<int>(mesh->mNumVertices); ++vi)
                {
                    for (int uvi = 0; uvi < mesh->mNumUVComponents[i]; uvi++)
                    {
                        float val = mesh->mTextureCoords[i][vi][uvi];
                        *reinterpret_cast<float*>(data + attributeStrideSize * vi + texCoordStrideOffsets[i] + uvi * 4) = val;
                    }
                }
            }
        }

        for (int i = 0; i < AI_MAX_NUMBER_OF_COLOR_SETS; ++i)
        {
            if (mesh->HasVertexColors(i))
            {
                for (int vi = 0; vi < static_cast<int>(mesh->mNumVertices); ++vi)
                {
                    *reinterpret_cast<float*>(data + attributeStrideSize * vi + vertexColorStrideOffsets[i]) = mesh->mColors[i][vi].r;
                    *reinterpret_cast<float*>(data + attributeStrideSize * vi + vertexColorStrideOffsets[i] + 4) = mesh->mColors[i][vi].g;
                    *reinterpret_cast<float*>(data + attributeStrideSize * vi + vertexColorStrideOffsets[i] + 8) = mesh->mColors[i][vi].b;
                    *reinterpret_cast<float*>(data + attributeStrideSize * vi + vertexColorStrideOffsets[i] + 12) = mesh->mColors[i][vi].a;
                }
            }
        }

        Skeleton skeleton;
        std::vector<int> vertexBoneIndexOffset(mesh->mNumVertices, 0);
        for (int boneIndex = 0; boneIndex < static_cast<int>(mesh->mNumBones); ++boneIndex)
        {
            skeleton.push_back({std::string(mesh->mBones[boneIndex]->mName.C_Str()), AiMatrixToGlm(mesh->mBones[boneIndex]->mOffsetMatrix)});
            for (int w = 0; w < static_cast<int>(mesh->mBones[boneIndex]->mNumWeights); w++)
            {
                auto& weight = mesh->mBones[boneIndex]->mWeights[w];
                int vertexId = weight.mVertexId;
                int index = vertexBoneIndexOffset[vertexId]++;
                if (index < 4)
                {
                    *reinterpret_cast<float*>(data + attributeStrideSize * vertexId + skeletonOffset + 4 * index) = boneIndex * 10 + weight.mWeight;
                }
            }
        }

        std::vector<uint32_t> indices;
        for (int faceIndex = 0; faceIndex < static_cast<int>(mesh->mNumFaces); ++faceIndex)
        {
            for (int i = 0; i < static_cast<int>(mesh->mFaces[faceIndex].mNumIndices); ++i)
            {
                indices.push_back(mesh->mFaces[faceIndex].mIndices[i]);
            }
        }

        OptimizeMesh(positions, attributeData, attributeStrideSize, indices);
        attributes.SetData(std::move(attributeData));

        Submesh submesh;
        submesh.SetPositions(std::move(positions));
        submesh.SetVertexAttribute(std::move(attributes));
        submesh.SetIndices(std::move(indices));
        AABB aabb;
        aabb.min = {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z};
        aabb.max = {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z};
        submesh.SetAABB(aabb);
        submesh.Apply();

        auto importedMesh = std::make_unique<Mesh>();
        importedMesh->SetName(fmt::format("{} {}", mesh->mName.C_Str(), meshIndex));
        importedMesh->SetSkeleton(std::move(skeleton));
        std::vector<Submesh> submeshes;
        submeshes.push_back(std::move(submesh));
        importedMesh->SetSubmeshes(std::move(submeshes));

        UUID artifactUUID = context.GetSubAssetUUID(
            importedMesh->GetName(),
            Mesh::StaticGetTypeName(),
            AssetArtifacts::Kind::Mesh,
            fmt::format("{}", meshIndex)
        );
        importedMesh->SetUUID(artifactUUID);
        auto relativePath = AssetArtifacts::MakeArtifactPath(artifactUUID, AssetArtifacts::Kind::Mesh);
        ModelArtifact::WriteMeshBlob(context.importDatabase->GetImportDatabaseRootPath() / relativePath, *importedMesh);
        context.artifacts.push_back({artifactUUID, context.sourceAssetUUID, std::string(AssetArtifacts::ToString(AssetArtifacts::Kind::Mesh)), importedMesh->GetName(), relativePath, false, fmt::format("mesh/{}", meshIndex)});
        context.meshes.push_back(std::move(importedMesh));
    }
}

bool ExtractTexture(ModelImportContext& context, std::unique_ptr<Material>& mat, aiMaterial* material, aiTextureType type, const char* bindingName, const char* keyword)
{
    if (material->GetTextureCount(type) == 0)
    {
        return false;
    }

    aiString texName;
    material->Get(AI_MATKEY_TEXTURE(type, 0), texName);
    if (Texture* embeddedTexture = context.TryImportEmbeddedTexture(texName))
    {
        mat->RawSetTexture(bindingName, embeddedTexture);
    }
    else
    {
        auto texturePath = context.absoluteAssetPath->parent_path() / texName.C_Str();
        auto tex = static_cast<Texture*>(AssetDatabase::Singleton()->LoadAsset(texturePath));
        if (tex == nullptr)
        {
            spdlog::warn(
                "ModelImporter: failed to load external texture '{}' for material binding '{}'.",
                texturePath.string(),
                bindingName
            );
        }
        mat->RawSetTexture(bindingName, tex);
    }

    mat->SetTextureSamplerIndex(bindingName, GetSamplerIndex(material, type, bindingName, texName));
    mat->EnableFeature(keyword);
    return true;
}

void ProcessMaterials(ModelImportContext& context)
{
    for (int materialIndex = 0; materialIndex < static_cast<int>(context.scene->mNumMaterials); ++materialIndex)
    {
        std::unique_ptr<Material> mat = std::make_unique<Material>();
        mat->SetShader(Shaders::SceneLit);
        aiMaterial* material = context.scene->mMaterials[materialIndex];
        std::string materialName = material->GetName().C_Str();
        if (materialName.empty())
        {
            materialName = fmt::format("Material {}", materialIndex);
        }
        mat->SetName(materialName);

        aiColor4D baseColorFactor = {0.5, 0.5, 0.5, 0.5};
        aiColor4D emissive = {0, 0, 0, 0};
        float roughness = 0.4f;
        float metallic = 0.2f;
        float alphaCutoff = 0.5f;
        aiString alphaMode;
        bool twoSided = false;
        material->Get(AI_MATKEY_BASE_COLOR, baseColorFactor);
        material->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissive);
        material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
        material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff);
        material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode);
        material->Get(AI_MATKEY_TWOSIDED, twoSided);

        ExtractTexture(context, mat, material, aiTextureType_DIFFUSE, "baseColorTex", "_BaseColorMap");
        ExtractTexture(context, mat, material, aiTextureType_NORMALS, "normalMap", "_NormalMap");
        ExtractTexture(context, mat, material, aiTextureType_METALNESS, "metallicRoughnessMap", "_MetallicRoughnessMap");
        if (ExtractTexture(context, mat, material, aiTextureType_EMISSIVE, "emissiveMap", "_EmissiveMap"))
        {
            emissive = {1, 1, 1, 1};
        }

        mat->SetVector("PBR", "baseColorFactor", {baseColorFactor.r, baseColorFactor.g, baseColorFactor.b, baseColorFactor.a});
        mat->SetVector("PBR", "emissive", {emissive.r, emissive.g, emissive.b, emissive.a});
        mat->SetFloat("PBR", "roughness", roughness);
        mat->SetFloat("PBR", "metallic", metallic);
        mat->SetFloat("PBR", "alphaCutoff", alphaCutoff);

        for (int meshIndex = 0; meshIndex < static_cast<int>(context.scene->mNumMeshes); ++meshIndex)
        {
            aiMesh* mesh = context.scene->mMeshes[meshIndex];
            if (mesh->mMaterialIndex == materialIndex && mesh->HasBones())
            {
                mat->SetShader(Shaders::SceneLitSkinned);
                break;
            }
        }

        auto shaderConfig = *mat->GetShader()->GetShaderProgram()->GetDefaultShaderConfig();
        shaderConfig.cullMode = twoSided ? Gfx::CullMode::None : Gfx::CullMode::Back;
        std::string alphaModel = alphaMode.C_Str();
        shaderConfig.depth.testEnable = true;
        if (shaderConfig.color.blends.empty())
        {
            shaderConfig.color.blends.push_back({});
        }
        if (alphaModel == "MASK")
        {
            mat->EnableFeature("_AlphaClip");
            shaderConfig.color.blends[0].blendEnable = false;
        }
        else if (alphaModel == "BLEND")
        {
            auto& blend = shaderConfig.color.blends[0];
            blend.blendEnable = true;
            blend.srcColorBlendFactor = Gfx::BlendFactor::Src_Alpha;
            blend.dstColorBlendFactor = Gfx::BlendFactor::One_Minus_Src_Alpha;
            blend.colorBlendOp = Gfx::BlendOp::Add;
            blend.srcAlphaBlendFactor = Gfx::BlendFactor::Src_Alpha;
            blend.dstAlphaBlendFactor = Gfx::BlendFactor::One_Minus_Src_Alpha;
            blend.alphaBlendOp = Gfx::BlendOp::Add;
            shaderConfig.depth.writeEnable = false;
        }
        else
        {
            mat->SetFloat("PBR", "alphaCutoff", 0.0f);
            shaderConfig.color.blends[0].blendEnable = false;
        }
        mat->SetShaderConfig(shaderConfig);

        UUID artifactUUID = context.GetSubAssetUUID(
            mat->GetName(),
            Material::StaticGetTypeName(),
            AssetArtifacts::Kind::Material,
            fmt::format("{}", materialIndex)
        );
        mat->SetUUID(artifactUUID);
        auto relativePath = AssetArtifacts::MakeArtifactPath(artifactUUID, AssetArtifacts::Kind::Material);
        JsonSerializer ser;
        mat->Serialize(&ser);
        auto binary = ser.GetBinary();
        std::ofstream out(context.importDatabase->GetImportDatabaseRootPath() / relativePath, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
        context.artifacts.push_back({artifactUUID, context.sourceAssetUUID, std::string(AssetArtifacts::ToString(AssetArtifacts::Kind::Material)), mat->GetName(), relativePath, false, fmt::format("material/{}", materialIndex)});
        context.materials.push_back(std::move(mat));
    }
}

void ProcessAnimations(ModelImportContext& context)
{
    for (size_t i = 0; i < context.scene->mNumAnimations; i++)
    {
        aiAnimation* clip = context.scene->mAnimations[i];
        std::string animationName = clip->mName.C_Str();
        if (animationName.empty())
        {
            animationName = "animation_" + std::to_string(i);
        }

        std::vector<Animation::Channel> channels;
        for (size_t ni = 0; ni < clip->mNumChannels; ni++)
        {
            auto node = context.scene->mRootNode->FindNode(clip->mChannels[ni]->mNodeName);
            if (node == nullptr)
            {
                continue;
            }

            Animation::Channel channel;
            channel.nodeName = clip->mChannels[ni]->mNodeName.C_Str();
            for (size_t ri = 0; ri < clip->mChannels[ni]->mNumPositionKeys; ri++)
            {
                auto& v = clip->mChannels[ni]->mPositionKeys[ri];
                channel.positions.emplace_back(v.mTime, glm::vec3(v.mValue.x, v.mValue.y, v.mValue.z));
            }
            for (size_t ri = 0; ri < clip->mChannels[ni]->mNumRotationKeys; ri++)
            {
                auto& v = clip->mChannels[ni]->mRotationKeys[ri];
                channel.rotations.emplace_back(v.mTime, glm::quat(v.mValue.w, v.mValue.x, v.mValue.y, v.mValue.z));
            }
            for (size_t ri = 0; ri < clip->mChannels[ni]->mNumScalingKeys; ri++)
            {
                auto& v = clip->mChannels[ni]->mScalingKeys[ri];
                channel.scalings.emplace_back(v.mTime, glm::vec3(v.mValue.x, v.mValue.y, v.mValue.z));
            }
            channels.push_back(std::move(channel));
        }

        auto animation = std::make_unique<Animation>();
        animation->SetName(animationName);
        animation->AddClip(animationName, static_cast<float>(clip->mTicksPerSecond), static_cast<float>(clip->mDuration), channels);

        UUID artifactUUID = context.GetSubAssetUUID(
            animation->GetName(),
            Animation::StaticGetTypeName(),
            AssetArtifacts::Kind::Animation,
            animationName
        );
        animation->SetUUID(artifactUUID);
        auto relativePath = AssetArtifacts::MakeArtifactPath(artifactUUID, AssetArtifacts::Kind::Animation);
        ModelArtifact::WriteAnimationBlob(context.importDatabase->GetImportDatabaseRootPath() / relativePath, *animation);
        context.artifacts.push_back({artifactUUID, context.sourceAssetUUID, std::string(AssetArtifacts::ToString(AssetArtifacts::Kind::Animation)), animationName, relativePath, false, fmt::format("animation/{}", animationName)});
        context.animations.push_back(std::move(animation));
    }
}

GameObject* ProcessNode(ModelImportContext& context, aiNode* node, GameObject* parent)
{
    auto gameObject = std::make_unique<GameObject>();
    GameObject* gameObjectPtr = gameObject.get();
    gameObject->SetName(node->mName.C_Str());
    ApplyTransform(*gameObject, AiMatrixToGlm(node->mTransformation));
    if (parent != nullptr)
    {
        gameObject->SetParent(parent, false);
    }

    if (node->mNumMeshes > 0)
    {
        std::vector<Mesh*> nodeMeshes;
        std::vector<Material*> nodeMaterials;
        for (int m = 0; m < static_cast<int>(node->mNumMeshes); ++m)
        {
            aiMesh* mesh = context.scene->mMeshes[node->mMeshes[m]];
            if (mesh->HasBones())
            {
                context.materials[mesh->mMaterialIndex]->SetShader(Shaders::SceneLitSkinned);
                context.materials[mesh->mMaterialIndex]->EnableFeature("_Vertex_Skeleton");
            }
            nodeMeshes.push_back(context.meshes[node->mMeshes[m]].get());
            nodeMaterials.push_back(context.materials[mesh->mMaterialIndex].get());
        }

        auto meshRenderer = gameObject->AddComponent<MeshRenderer>();
        gameObject->AddComponent<PhysicsBody>();
        meshRenderer->SetMeshes(nodeMeshes);
        meshRenderer->SetMaterials(nodeMaterials);
    }

    context.gameObjects.push_back(std::move(gameObject));

    for (int i = 0; i < static_cast<int>(node->mNumChildren); ++i)
    {
        ProcessNode(context, node->mChildren[i], gameObjectPtr);
    }

    return gameObjectPtr;
}

void ProcessModelGraph(ModelImportContext& context)
{
    GameObject* root = ProcessNode(context, context.scene->mRootNode, nullptr);
    context.roots.push_back(root);

    for (auto& animation : context.animations)
    {
        auto animationPlayer = root->AddComponent<AnimationPlayer>();
        animationPlayer->SetAnimation(animation.get());
    }

    UUID artifactUUID = context.sourceAssetUUID;
    auto relativePath = AssetArtifacts::MakeArtifactPath(artifactUUID, AssetArtifacts::Kind::Model);
    ModelArtifact::WriteModelGraph(context.importDatabase->GetImportDatabaseRootPath() / relativePath, context.gameObjects, context.roots);
    context.artifacts.push_back({artifactUUID, context.sourceAssetUUID, std::string(AssetArtifacts::ToString(AssetArtifacts::Kind::Model)), "main", relativePath, true, "model/main"});
}
} // namespace

const std::vector<std::type_index>& ModelImporter::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Model)};
    return types;
}

bool ModelImporter::ImportNeeded()
{
    ImportDatabase::ImportState state{};
    std::filesystem::path modelArtifactPath;
    const auto currentWriteTime = static_cast<uint64_t>(std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count());
    const auto currentMetaHash = ComputeMetaHash(meta);
    const auto currentContentHash = ComputeContentHash(absoluteAssetPath);

    if (!importDatabase->TryGetImportState(assetUUID.ToString(), state))
    {
        return true;
    }

    if (!importDatabase->TryGetArtifactPath(assetUUID.ToString(), AssetArtifacts::Kind::Model, modelArtifactPath))
    {
        return true;
    }

    return state.sourceWriteTime != currentWriteTime || state.metaHash != currentMetaHash ||
           state.contentHash != currentContentHash || !importDatabase->ArtifactExists(modelArtifactPath);
}

std::vector<std::filesystem::path> ModelImporter::Import()
{
    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    const aiScene* scene = importer.ReadFile(
        absoluteAssetPath.string().c_str(),
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_GenUVCoords |
            aiProcess_CalcTangentSpace | aiProcess_GenBoundingBoxes
    );

    if (scene == nullptr)
    {
        spdlog::error(importer.GetErrorString());
        return {};
    }

    importDatabase->DeleteAssetRows(assetUUID.ToString());
    importDatabase->UpsertImportState(
        assetUUID.ToString(),
        ImportDatabase::ImportState{
            static_cast<uint64_t>(std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count()),
            ComputeMetaHash(meta),
            ComputeContentHash(absoluteAssetPath)
        }
    );

    ModelImportContext context;
    context.scene = scene;
    context.absoluteAssetPath = &absoluteAssetPath;
    context.importDatabase = importDatabase;
    context.internalNameToUUID = internalNameToUUID;
    context.sourceAssetUUID = assetUUID;

    ProcessMeshes(context);
    ProcessMaterials(context);
    ProcessAnimations(context);
    ProcessModelGraph(context);

    for (const auto& artifact : context.artifacts)
    {
        importDatabase->ReplaceArtifact(
            artifact.sourceAssetUUID,
            artifact.artifactUUID,
            artifact.kind,
            artifact.name,
            artifact.relativePath,
            artifact.isMain,
            artifact.locator
        );
    }

    importDatabase->UpsertImportState(
        assetUUID.ToString(),
        ImportDatabase::ImportState{
            static_cast<uint64_t>(std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count()),
            ComputeMetaHash(meta),
            ComputeContentHash(absoluteAssetPath)
        }
    );

    return {};
}
