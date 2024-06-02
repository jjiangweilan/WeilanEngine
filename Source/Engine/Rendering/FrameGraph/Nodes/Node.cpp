#include "Node.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"

namespace Rendering::FrameGraph
{
void Node::Serialize(Serializer* s) const
{
    for (auto& c : configs)
    {
        ConfigurableType type = c.type;
        if (type == ConfigurableType::Bool)
        {
            bool v = std::any_cast<bool>(c.data);
            s->Serialize(c.name, (int32_t)v);
        }
        else if (type == ConfigurableType::Int)
        {
            int v = std::any_cast<int>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Float)
        {
            float v = std::any_cast<float>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Format)
        {
            int v = static_cast<int>(std::any_cast<Gfx::ImageFormat>(c.data));
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec2)
        {
            glm::vec2 v = std::any_cast<glm::vec2>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec3)
        {
            glm::vec3 v = std::any_cast<glm::vec3>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec4)
        {
            glm::vec4 v = std::any_cast<glm::vec4>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec2Int)
        {
            glm::vec2 v = std::any_cast<glm::ivec2>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec3Int)
        {
            glm::vec3 v = std::any_cast<glm::ivec3>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec4Int)
        {
            glm::vec4 v = std::any_cast<glm::ivec4>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::ObjectPtr)
        {
            Object* v = std::any_cast<Object*>(c.data);
            s->Serialize(c.name, v);
        }
    }
    // s->Serialize("inputProperties", inputProperties);
    // s->Serialize("outputProperties", outputProperties);
    s->Serialize("id", id);
    s->Serialize("name", name);
    s->Serialize("customName", customName);
}

void Node::Deserialize(Serializer* s)
{
    for (auto& c : configs)
    {
        ConfigurableType type = c.type;
        if (type == ConfigurableType::Bool)
        {
            int32_t v = 0;
            s->Deserialize(c.name, v);
            c.data = (bool)v;
        }
        else if (type == ConfigurableType::Int)
        {
            int v = 0;
            s->Deserialize(c.name, v);
            c.data = v;
        }
        else if (type == ConfigurableType::Float)
        {
            float v = 0;
            s->Deserialize(c.name, v);
            c.data = v;
        }
        else if (type == ConfigurableType::Format)
        {
            int v = 0;
            s->Deserialize(c.name, v);
            c.data = static_cast<Gfx::ImageFormat>(v);
        }
        else if (type == ConfigurableType::Vec2)
        {
            glm::vec2 v;
            s->Deserialize(c.name, v);
            c.data = v;
        }
        else if (type == ConfigurableType::Vec3)
        {
            glm::vec3 v;
            s->Deserialize(c.name, v);
            c.data = v;
        }
        else if (type == ConfigurableType::Vec4)
        {
            glm::vec4 v;
            s->Deserialize(c.name, v);
            c.data = v;
        }
        else if (type == ConfigurableType::Vec2Int)
        {
            glm::vec2 v;
            s->Deserialize(c.name, v);
            c.data = glm::ivec2(v);
        }
        else if (type == ConfigurableType::Vec3Int)
        {
            glm::vec3 v;
            s->Deserialize(c.name, v);
            c.data = glm::ivec3(v);
        }
        else if (type == ConfigurableType::Vec4Int)
        {
            glm::vec4 v;
            s->Deserialize(c.name, v);
            c.data = glm::ivec4(v);
        }
        else if (type == ConfigurableType::ObjectPtr)
        {
            s->Deserialize(c.name, c.dataRefHolder, [&c](void* res) { c.data = c.dataRefHolder; });
        }
    }
    // s->Serialize("inputProperties", inputProperties);
    // s->Serialize("outputProperties", outputProperties);
    s->Deserialize("id", id);

    // fix property id
    for (auto& p : inputProperties)
    {
        p.id += GetID();
        inputPropertyIDs[p.GetName()] = p.id;
    }
    for (auto& p : outputProperties)
    {
        p.id += GetID();
        outputPropertyIDs[p.GetName()] = p.id;
    }

    s->Deserialize("name", name);
    s->Deserialize("customName", customName);
}

void swap(SceneObjectDrawData&& a, SceneObjectDrawData&& b)
{
    SceneObjectDrawData c(std::move(a));
    a = std::move(b);
    b = std::move(c);
}

void DrawList::Add(MeshRenderer& meshRenderer)
{
    auto mesh = meshRenderer.GetMesh();
    if (mesh == nullptr)
        return;

    auto& submeshes = mesh->GetSubmeshes();
    auto& materials = meshRenderer.GetMaterials();

    if (!meshRenderer.IsMultipassEnabled())
    {
        for (int i = 0; i < submeshes.size() || i < materials.size(); ++i)
        {
            auto material = i < materials.size() ? materials[i] : nullptr;
            auto submesh = i < submeshes.size() ? &submeshes[i] : nullptr;
            auto shader = material ? material->GetShader() : nullptr;

            if (submesh != nullptr && material != nullptr && shader != nullptr)
            {
                uint32_t indexCount = submesh->GetIndexCount();

                SceneObjectDrawData drawData;
                drawData.vertexBufferBinding = std::vector<Gfx::VertexBufferBinding>();
                for (auto& binding : submesh->GetBindings())
                {
                    drawData.vertexBufferBinding.push_back({submesh->GetVertexBuffer(), binding.byteOffset});
                }
                drawData.indexBuffer = submesh->GetIndexBuffer();
                drawData.indexBufferType = submesh->GetIndexBufferType();

                material->UploadDataToGPU();
                drawData.shaderResource = material->GetShaderResource();
                drawData.shader = (Shader*)shader;
                drawData.shaderConfig = &material->GetShaderConfig();
                auto modelMatrix = meshRenderer.GetGameObject()->GetModelMatrix();
                drawData.pushConstant = modelMatrix;
                drawData.indexCount = indexCount;
                drawData.material = material;

                push_back(std::move(drawData));
            }
        }
    }
    else
    {
        for (int i = 0; i < materials.size(); ++i)
        {
            auto material = materials[i];
            if (material == nullptr)
                continue;

            auto shader = material ? material->GetShaderProgram() : nullptr;
            material->UploadDataToGPU();
            for (auto& submesh : submeshes)
            {
                if (material != nullptr && shader != nullptr)
                {
                    // material->SetMatrix("Transform", "model",
                    // meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix());
                    uint32_t indexCount = submesh.GetIndexCount();

                    SceneObjectDrawData drawData;
                    drawData.vertexBufferBinding = std::vector<Gfx::VertexBufferBinding>();
                    for (auto& binding : submesh.GetBindings())
                    {
                        drawData.vertexBufferBinding.push_back({submesh.GetVertexBuffer(), binding.byteOffset});
                    }
                    drawData.indexBuffer = submesh.GetIndexBuffer();
                    drawData.indexBufferType = submesh.GetIndexBufferType();

                    drawData.shaderResource = material->GetShaderResource();
                    drawData.shader = (Shader*)shader;
                    drawData.shaderConfig = &material->GetShaderConfig();
                    auto modelMatrix = meshRenderer.GetGameObject()->GetModelMatrix();
                    drawData.pushConstant = modelMatrix;
                    drawData.indexCount = indexCount;

                    push_back(std::move(drawData));
                }
            }
        }
    }
}

} // namespace Rendering::FrameGraph
