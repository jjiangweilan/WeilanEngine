#include "ModelLoader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "Core/Model.hpp"
#include "Rendering/Animation.hpp"
#include "Rendering/ShaderLibrary.hpp"
#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
DEFINE_ASSET_LOADER(ModelLoader, "glb,gltf,fbx")

struct ModelImporterImple
{
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::unique_ptr<Texture>> textures;
    std::vector<std::unique_ptr<Material>> materials;
    std::vector<std::unique_ptr<Animation>> animations;
    ModelNode rootNode;

    std::filesystem::path absoluteAssetPath;

    glm::mat4 aiMatrixToGlm(aiMatrix4x4 m)
    {
        m = m.Transpose();
        glm::mat4 glmM = {
            m.a1,
            m.a2,
            m.a3,
            m.a4,
            m.b1,
            m.b2,
            m.b3,
            m.b4,
            m.c1,
            m.c2,
            m.c3,
            m.c4,
            m.d1,
            m.d2,
            m.d3,
            m.d4,
        };

        return glmM;
    }

    void Load(const std::filesystem::path& path)
    {
        this->absoluteAssetPath = path;
        Assimp::Importer importer;
        importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
        scene = importer.ReadFile(
            path.string().c_str(),
            aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_GenUVCoords |
                aiProcess_CalcTangentSpace | aiProcess_GenBoundingBoxes | aiProcess_OptimizeMeshes
        );

        if (scene == nullptr)
        {
            spdlog::error(importer.GetErrorString());
            return;
        }

        ProcessMesh();
        ProcessMaterial();
        ProcessAnimation();

        rootNode = ProcessNode(scene->mRootNode);
    }

private:
    ModelNode ProcessNode(aiNode* node)
    {
        ModelNode modelNode;
        modelNode.name = node->mName.C_Str();
        for (int m = 0; m < node->mNumMeshes; ++m)
        {
            if (scene->mMeshes[node->mMeshes[m]]->HasBones())
            {
                materials[scene->mMeshes[node->mMeshes[m]]->mMaterialIndex]->SetShader(ShaderLibrary::SceneLitSkinned);
                materials[scene->mMeshes[node->mMeshes[m]]->mMaterialIndex]->EnableFeature("_Vertex_Skeleton");
            }
            modelNode.meshes.push_back({node->mMeshes[m], scene->mMeshes[node->mMeshes[m]]->mMaterialIndex});
        }

        modelNode.transform = aiMatrixToGlm(node->mTransformation);

        for (int i = 0; i < node->mNumChildren; ++i)
        {
            modelNode.children.push_back(ProcessNode(node->mChildren[i]));
        }

        return modelNode;
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
            uint32_t skeletonOffset = 0;
            if (mesh->HasNormals())
            {
                attributeStrideSize += normalSize;
                attributes.AddAttribute("NORMAL0", VertexAttributeSemantics::Normal, 0, normalSize);
            }

            if (mesh->HasTangentsAndBitangents())
            {
                tangentStrideOffset = attributeStrideSize;
                attributeStrideSize += tangentSize; // we only use tangent
                attributes.AddAttribute("TANGENT0", VertexAttributeSemantics::Tangent, 0, tangentSize);
            }

            const char* texCoordNames[8] = {
                "TEXCOORD0",
                "TEXCOORD1",
                "TEXCOORD2",
                "TEXCOORD3",
                "TEXCOORD4",
                "TEXCOORD5",
                "TEXCOORD6",
                "TEXCOORD7"
            };
            const int MaxTexcoordChannels = 1;
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

            const char* vertexColorNames[8] =
                {"COLOR0", "COLOR1", "COLOR2", "COLOR3", "COLOR4", "COLOR5", "COLOR6", "COLOR7"};
            const uint32_t vertexColorSize = 16;
            for (int i = 0; i < AI_MAX_NUMBER_OF_COLOR_SETS; ++i)
            {
                if (mesh->HasVertexColors(i))
                {
                    vertexColorStrideOffsets[i] = attributeStrideSize;
                    attributeStrideSize += vertexColorSize;
                    attributes.AddAttribute(vertexColorNames[i], VertexAttributeSemantics::Color, i, vertexColorSize);
                }
            }

            const uint32_t boneSize = 16; // packed id + weight
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
                for (int i = 0; i < mesh->mNumVertices; ++i)
                {
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + normalStrideOffset) =
                        mesh->mNormals[i].x;
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + normalStrideOffset + 4) =
                        mesh->mNormals[i].y;
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + normalStrideOffset + 8) =
                        mesh->mNormals[i].z;
                }
            }

            if (mesh->HasTangentsAndBitangents())
            {
                for (int i = 0; i < mesh->mNumVertices; ++i)
                {
                    auto tangent = mesh->mTangents[i];
                    auto bitangent = mesh->mBitangents[i];
                    auto normal = mesh->mNormals[i];
                    glm::vec3 glmTangent = {tangent.x, tangent.y, tangent.z};
                    glm::vec3 glmBitangent = {bitangent.x, bitangent.y, bitangent.z};
                    glm::vec3 glmNormal = {normal.x, normal.y, normal.z};
                    float w = glm::sign(dot(glm::cross(glmTangent, glmBitangent), glmNormal));
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset) =
                        mesh->mTangents[i].x;
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset + 4) =
                        mesh->mTangents[i].y;
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset + 8) =
                        mesh->mTangents[i].z;
                    *reinterpret_cast<float*>(data + attributeStrideSize * i + tangentStrideOffset + 12) = w;
                }
            }

            for (int i = 0; i < MaxTexcoordChannels; ++i)
            {
                if (mesh->HasTextureCoords(i))
                {
                    for (int vi = 0; vi < mesh->mNumVertices; ++vi)
                    {
                        for (int uvi = 0; uvi < mesh->mNumUVComponents[i]; uvi++)
                        {
                            *reinterpret_cast<float*>(
                                data + attributeStrideSize * vi + texCoordStrideOffsets[i] + uvi * 4
                            ) = mesh->mTextureCoords[i][vi][uvi];
                        }
                    }
                }
            }

            for (int i = 0; i < AI_MAX_NUMBER_OF_COLOR_SETS; ++i)
            {
                if (mesh->HasVertexColors(i))
                {
                    for (int vi = 0; vi < mesh->mNumVertices; ++vi)
                    {
                        *reinterpret_cast<float*>(data + attributeStrideSize * vi + vertexColorStrideOffsets[i]) =
                            mesh->mColors[i][vi].r;
                        *reinterpret_cast<float*>(data + attributeStrideSize * vi + vertexColorStrideOffsets[i] + 4) =
                            mesh->mColors[i][vi].g;
                        *reinterpret_cast<float*>(data + attributeStrideSize * vi + vertexColorStrideOffsets[i] + 8) =
                            mesh->mColors[i][vi].b;
                        *reinterpret_cast<float*>(data + attributeStrideSize * vi + vertexColorStrideOffsets[i] + 12) =
                            mesh->mColors[i][vi].a;
                    }
                }
            }

            Skeleton skeleton;
            std::vector<int> vertexBoneIndexOffset(mesh->mNumVertices, 0);
            for (int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
            {
                skeleton.push_back(
                    {std::string(mesh->mBones[boneIndex]->mName.C_Str()),
                     aiMatrixToGlm(mesh->mBones[boneIndex]->mOffsetMatrix)}
                );
                for (int w = 0; w < mesh->mBones[boneIndex]->mNumWeights; w++)
                {
                    auto& weight = mesh->mBones[boneIndex]->mWeights[w];
                    int vertexId = weight.mVertexId;
                    int index = vertexBoneIndexOffset[vertexId]++;
                    if (index < 4)
                    {
                        *reinterpret_cast<float*>(data + attributeStrideSize * vertexId + skeletonOffset + 4 * index) =
                            boneIndex * 10 + weight.mWeight;
                    }
                }
            }

            attributes.SetData(std::move(attributeData));

            std::vector<uint32_t> indices;
            for (int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
            {
                for (int i = 0; i < mesh->mFaces[faceIndex].mNumIndices; ++i)
                {
                    indices.push_back(mesh->mFaces[faceIndex].mIndices[i]);
                }
            }

            Submesh submesh;
            std::unique_ptr<Mesh> myMesh = std::make_unique<Mesh>();
            submesh.SetPositions(std::move(positions));
            submesh.SetVertexAttribute(std::move(attributes));
            submesh.SetIndices(std::move(indices));
            AABB aabb;
            aabb.min = {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z},
            aabb.max = {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z};
            submesh.SetAABB(aabb);
            submesh.Apply();
            std::vector<Submesh> submeshes;
            submeshes.push_back(std::move(submesh));
            myMesh->SetSubmeshes(std::move(submeshes));
            myMesh->SetName(fmt::format("{} {}", mesh->mName.C_Str(), meshIndex));
            myMesh->SetSkeleton(skeleton);
            this->meshes.push_back(std::move(myMesh));
        }
    }

    bool ExtractTexture(
        std::unique_ptr<Material>& mat,
        aiMaterial*& material,
        aiTextureType type,
        const char* bindingName,
        const char* keyword
    )
    {
        if (material->GetTextureCount(type) > 0)
        {
            aiString texName;
            material->Get(AI_MATKEY_TEXTURE(type, 0), texName);

            auto tex = AssetDatabase::Singleton()->LoadAssetAsync_Experimental(
                std::filesystem::relative(
                    absoluteAssetPath.parent_path() / texName.C_Str(),
                    AssetDatabase::Singleton()->GetAssetDirectory()
                )
            );

            mat->RawSetTexture(bindingName, ObjPtr<Texture>(std::move(tex)));
            mat->EnableFeature(keyword);
            return true;
        }

        return false;
    }

    void ProcessMaterial()
    {
        // preload all textures
        std::set<std::filesystem::path> texturePaths{};
        aiTextureType textureTypes[] =
            {aiTextureType_DIFFUSE, aiTextureType_NORMALS, aiTextureType_METALNESS, aiTextureType_EMISSIVE};
        for (int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
        {
            for (int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
            {
                auto material = scene->mMaterials[materialIndex];
                for (int i = 0; i < sizeof(textureTypes) / sizeof(aiTextureType); ++i)
                {
                    if (material->GetTextureCount(textureTypes[i]) > 0)
                    {
                        aiString texName;
                        material->Get(AI_MATKEY_TEXTURE(textureTypes[i], 0), texName);
                        texturePaths.insert(
                            std::filesystem::relative(
                                absoluteAssetPath.parent_path() / texName.C_Str(),
                                AssetDatabase::Singleton()->GetAssetDirectory()
                            )
                        );
                    }
                }
            }
        }
        std::vector<std::filesystem::path> texturePathsAsVec(texturePaths.begin(), texturePaths.end());
        for (const auto& p : texturePathsAsVec)
        {
            AssetDatabase::Singleton()->LoadAsset(p);
        }

        for (int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
        {
            std::unique_ptr<Material> mat = std::make_unique<Material>();
            mat->SetShader(ShaderLibrary::SceneLit);
            auto material = scene->mMaterials[materialIndex];
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
            bool twoSided;
            material->Get(AI_MATKEY_BASE_COLOR, baseColorFactor);
            material->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissive);
            material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
            material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
            material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff);
            material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode);
            material->Get(AI_MATKEY_TWOSIDED, twoSided);
            ExtractTexture(mat, material, aiTextureType_DIFFUSE, "baseColorTex", "_BaseColorMap");
            ExtractTexture(mat, material, aiTextureType_NORMALS, "normalMap", "_NormalMap");
            ExtractTexture(mat, material, aiTextureType_METALNESS, "metallicRoughnessMap", "_MetallicRoughnessMap");
            if (ExtractTexture(mat, material, aiTextureType_EMISSIVE, "emissiveMap", "_EmissiveMap"))
            {
                emissive = {1, 1, 1, 1};
            }

            mat->SetVector(
                "PBR",
                "baseColorFactor",
                {baseColorFactor.r, baseColorFactor.g, baseColorFactor.b, baseColorFactor.a}
            );
            mat->SetVector("PBR", "emissive", {emissive.r, emissive.g, emissive.b, emissive.a});
            mat->SetFloat("PBR", "roughness", roughness);
            mat->SetFloat("PBR", "metallic", metallic);
            mat->SetFloat("PBR", "alphaCutoff", alphaCutoff);

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
                mat->SetFloat("PBR", "alphaCutoff", alphaCutoff);
                mat->EnableFeature("_AlphaTest");
                auto& blend = shaderConfig.color.blends[0];
                blend.blendEnable = false;
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
                auto& blend = shaderConfig.color.blends[0];
                blend.blendEnable = false;
            }
            mat->SetShaderConfig(shaderConfig);

            materials.push_back(std::move(mat));
        }
    }

    void ProcessAnimation()
    {
        if (scene->mNumAnimations == 0)
            return;

        auto myAnimation = std::make_unique<Animation>();
        for (size_t i = 0; i < scene->mNumAnimations; i++)
        {
            auto clip = scene->mAnimations[i];
            std::vector<Animation::Channel> channels;
            for (size_t ni = 0; ni < clip->mNumChannels; ni++)
            {
                auto node = scene->mRootNode->FindNode(clip->mChannels[ni]->mNodeName);
                if (node == nullptr)
                    continue;

                Animation::Channel channel;
                channel.nodeName = clip->mChannels[ni]->mNodeName.C_Str();

                for (size_t ri = 0; ri < clip->mChannels[ni]->mNumPositionKeys; ri++)
                {
                    auto& v = clip->mChannels[ni]->mPositionKeys[ri];
                    float x = v.mValue.x; // v.mValue.x > 0.99999 ? 1 : v.mValue.x;
                    float y = v.mValue.y; // v.mValue.y > 0.99999 ? 1 : v.mValue.y;
                    float z = v.mValue.z; // v.mValue.z > 0.99999 ? 1 : v.mValue.z;
                    channel.positions.emplace_back(v.mTime, glm::vec3(x, y, z));
                }
                for (size_t ri = 0; ri < clip->mChannels[ni]->mNumRotationKeys; ri++)
                {
                    auto& v = clip->mChannels[ni]->mRotationKeys[ri];
                    float x = v.mValue.x; // v.mValue.x > 0.99999 ? 1 : v.mValue.x;
                    float y = v.mValue.y; // v.mValue.y > 0.99999 ? 1 : v.mValue.y;
                    float z = v.mValue.z; // v.mValue.z > 0.99999 ? 1 : v.mValue.z;
                    float w = v.mValue.w; // v.mValue.w > 0.99999 ? 1 : v.mValue.w;
                    channel.rotations.emplace_back(v.mTime, glm::quat(w, x, y, z));
                }
                for (size_t ri = 0; ri < clip->mChannels[ni]->mNumScalingKeys; ri++)
                {
                    auto& v = clip->mChannels[ni]->mScalingKeys[ri];
                    float x = v.mValue.x; // v.mValue.x > 0.99999 ? 1 : v.mValue.x;
                    float y = v.mValue.y; // v.mValue.y > 0.99999 ? 1 : v.mValue.y;
                    float z = v.mValue.z; // v.mValue.z > 0.99999 ? 1 : v.mValue.z;
                    channel.scalings.emplace_back(v.mTime, glm::vec3(x, y, z));
                }
                channels.push_back(channel);
            }

            std::string clipName = clip->mName.C_Str();
            if (clipName.empty())
            {
                clipName = fmt::format("clip_{}", i);
            }

            std::string animationName = clip->mName.C_Str();
            if (animationName.empty())
            {
                animationName = "animation_" + std::to_string(i);
            }

            myAnimation->clips[animationName] = std::make_unique<Animation::AnimationClip>(
                animationName,
                (float)clip->mTicksPerSecond,
                (float)clip->mDuration,
                channels
            );
        }
        myAnimation->SetName("AnimationCollection");
        animations.push_back(std::move(myAnimation));
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
    ModelImporterImple e;
    e.Load(absoluteAssetPath);

    auto model = std::make_unique<Model>();
    model->SetModel(
        std::move(e.rootNode),
        std::move(e.meshes),
        std::move(e.textures),
        std::move(e.materials),
        std::move(e.animations)
    );

    asset = std::move(model);
}
