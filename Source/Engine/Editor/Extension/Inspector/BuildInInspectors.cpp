#include "BuildInInspectors.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "../../imgui/imgui.h"
#include "../../EditorRegister.hpp"
#include "../../ProjectManagement/ProjectManagement.hpp"
#include "Core/Graphics/Shader.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "Core/GameScene/GameSceneManager.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "Utils/EnumStringMapping.hpp"

namespace Engine::Editor
{
#define REGISTER(Name) EditorRegister::Instance()->RegisterInspector<Name, Name##Inspector>();

    void InitializeBuiltInInspector()
    {
        REGISTER(Material);
        REGISTER(MeshRenderer);
        REGISTER(Transform);
        REGISTER(GameObject);
        REGISTER(Shader);
        REGISTER(GameScene);
    }

    void MaterialInspector::Tick(RefPtr<EditorContext> editorContext)
    {
        RefPtr<Material> mat = target;
        RefPtr<Shader> shader = mat->GetShader();

        char goNameArea[256];
        auto& goName = mat->GetName();
        int matNameSize = goName.size() < 255 ? goName.size() : 255;
        strncpy(goNameArea, goName.c_str(), matNameSize + 1);
        ImGui::Text("%s", "Name: ");
        ImGui::SameLine();
        if (ImGui::InputText("##GameObjectName", goNameArea, 255))
        {
            mat->SetName(goNameArea);
        }

        std::string shaderName = "";
        if (shader)
        {
            shaderName = shader->GetName();
        }

        char shaderNameText[256];
        int shaderNameSize = shaderName.size() < 255 ? shaderName.size() : 255;
        strncpy(shaderNameText, shaderName.c_str(), shaderNameSize + 1);
        ImGui::Text("%s", "Shader: ");
        ImGui::SameLine();
        if (ImGui::InputText("##ShaderNameText", shaderNameText, 256))
        {
            mat->SetShader(shaderNameText);
        }
    }

    void GameSceneInspector::Tick(RefPtr<EditorContext> editorContext)
    {
        char goNameArea[256];
        auto& goName = target->GetName();
        int shaderNameSize = goName.size() < 255 ? goName.size() : 255;
        strncpy(goNameArea, goName.c_str(), shaderNameSize + 1);
        ImGui::Text("%s", "Name: ");
        ImGui::SameLine();
        if (ImGui::InputText("##SceneName", goNameArea, 255))
        {
            target->SetName(goNameArea);
        }
        if (ImGui::Button("Set as Active"))
        {
            GameSceneManager::Instance()->SetActiveGameScene(target);
            ProjectManagement::instance->SetLastActiveScene(target);
        }
    }

    void TransformInspector::Tick(RefPtr<EditorContext> editorContext)
    {
        RefPtr<Transform> tsm = target;
        glm::vec3 pos = tsm->GetPosition();
        ImGui::Text("Position"); ImGui::SameLine();
        if(ImGui::DragFloat3("##Position", &pos.x, 0.01f))
        {
            tsm->SetPostion(pos);
        }

        glm::vec3 scale = tsm->GetScale();
        ImGui::Text("Scale"); ImGui::SameLine();
        if(ImGui::DragFloat3("##Scale", &scale.x, 0.01f))
        {
            tsm->SetScale(scale);
        }

        glm::vec3 rotation = glm::degrees(tsm->GetRotation());
        ImGui::Text("Rotation"); ImGui::SameLine();
        if(ImGui::DragFloat3("##Rotation", &rotation.x, 0.1f))
        {
            tsm->SetRotation(glm::radians(rotation));
        }
    }

    void MeshRendererInspector::Tick(RefPtr<EditorContext> editorContext)
    {
        RefPtr<MeshRenderer> meshRenderer = target;

        Mesh* mesh = meshRenderer->GetMesh();
        if (mesh)
            ImGui::Button(("Mesh: " + mesh->GetName()).c_str());
        else
            ImGui::Button("Mesh: null");
        if (ImGui::BeginDragDropTarget())
        {
#define DYNAMIC_CAST_PAYLOAD(data, Type) dynamic_cast<Type*>(*(AssetObject**)data)

            auto payload = ImGui::GetDragDropPayload();
            if (Mesh* asMesh = DYNAMIC_CAST_PAYLOAD(payload->Data, Mesh))
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GameEditorDNDPayload"))
                {
                    meshRenderer->SetMesh(asMesh);
                }
            }
            ImGui::EndDragDropTarget();
        }

        Material* mat = meshRenderer->GetMaterial();
        if (mat)
            ImGui::Button(("Material: " + mat->GetName()).c_str());
        else
            ImGui::Button("Material: null");
        if (ImGui::BeginDragDropTarget())
        {
#define DYNAMIC_CAST_PAYLOAD(data, Type) dynamic_cast<Type*>(*(AssetObject**)data)

            auto payload = ImGui::GetDragDropPayload();
            if (Material* asMaterial = DYNAMIC_CAST_PAYLOAD(payload->Data, Material))
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GameEditorDNDPayload"))
                {
                    meshRenderer->SetMaterial(asMaterial);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }


    void GameObjectInspector::Tick(RefPtr<EditorContext> editorContext)
    {
        RefPtr<GameObject> activeObject = target;
        auto& components = activeObject->GetComponents();
        uint32_t id = 0;
        char goNameArea[256];
        auto& goName = activeObject->GetName();
        int shaderNameSize = goName.size() < 255 ? goName.size() : 255;
        strncpy(goNameArea, goName.c_str(), shaderNameSize + 1);
        ImGui::Text("%s", "Name: ");
        ImGui::SameLine();
        if (ImGui::InputText("##GameObjectName", goNameArea, 255))
        {
            activeObject->SetName(goNameArea);
        }
        for(auto& comp : components)
        {
            std::string title = comp->GetName() + "##" + std::to_string(id);
            if (ImGui::TreeNode(title.c_str()))
            {
                auto inspector = EditorRegister::Instance()->GetInspector(comp.Get());
                if (inspector != nullptr)
                    inspector->Tick(editorContext);

                ImGui::TreePop();
            }
        }

        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::TreeNode("Create Component"))
            {
                if (ImGui::Selectable("Camera"))
                {
                    activeObject->AddComponent<Camera>();
                }
                if (ImGui::Selectable("MeshRenderer"))
                {
                    activeObject->AddComponent<MeshRenderer>();
                }
                ImGui::TreePop();
            }

            ImGui::EndPopup();
        }
    }

    void ShaderInspector::Tick(RefPtr<EditorContext> editorContext)
    {
        RefPtr<Shader> shader = target;
        auto& config = shader->GetDefaultShaderConfig();

        shader->GetDefaultShaderConfig();
        ImGui::Text("Name: %s", shader->GetName().c_str());

        for(auto& blend : config.color.blends)
        {
            ImGui::Text("Blend: %s", blend.blendEnable ? "true" : "false");
            if (blend.blendEnable)
            {
                ImGui::Text("srcColorBlendFactor: %s", Utils::MapStrBlendFactor(blend.srcColorBlendFactor));
                ImGui::Text("srcAlphaBlendFactor: %s", Utils::MapStrBlendFactor(blend.srcAlphaBlendFactor));
                ImGui::Text("dstColorBlendFactor: %s", Utils::MapStrBlendFactor(blend.dstColorBlendFactor));
                ImGui::Text("dstAlphaBlendFactor: %s", Utils::MapStrBlendFactor(blend.dstAlphaBlendFactor));
                ImGui::Text("alphaBlendOp: %s", Utils::MapStrBlendOp(blend.alphaBlendOp));
                ImGui::Text("colorBlendOp: %s", Utils::MapStrBlendOp(blend.colorBlendOp));
            }
        }
    }
}
