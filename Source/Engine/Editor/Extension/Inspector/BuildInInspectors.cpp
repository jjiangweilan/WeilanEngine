#include "BuildInInspectors.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Component/LuaScript.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "../../EditorRegister.hpp"
#include "../../ProjectManagement/ProjectManagement.hpp"
#include "Core/Graphics/Shader.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Texture.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "Core/GameScene/GameSceneManager.hpp"
#include "Core/Rendering/RenderPipeline.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "Utils/EnumStringMapping.hpp"

#define DYNAMIC_CAST_PAYLOAD(data, Type) dynamic_cast<Type*>(*(AssetObject**)data)
namespace Engine::Editor
{
#define REGISTER(Name) EditorRegister::Instance()->RegisterInspector<Name, Name##Inspector>();

    template<class T>
    void AssetObjectField(const char* name, RefPtr<T>& assetObject)
    {
        ImGui::Text("%s", name);
        ImGui::SameLine();
        const char* buttonName = "null";
        if (assetObject != nullptr) buttonName = assetObject->GetName().c_str();
        ImGui::Button(buttonName);
        if (ImGui::BeginDragDropTarget())
        {
            auto payload = ImGui::GetDragDropPayload();
            if (AssetObject* aobj = DYNAMIC_CAST_PAYLOAD(payload->Data, AssetObject))
            {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GameEditorDNDPayload");
                if ((aobj->GetTypeInfo() == typeid(T)) && payload)
                {
                    ImGui::EndDragDropTarget();
                    assetObject = (T*)aobj;
                    return;
                }
            }
            ImGui::EndDragDropTarget();
        }

    }

    void InitializeBuiltInInspector()
    {
        REGISTER(Material);
        REGISTER(MeshRenderer);
        REGISTER(Transform);
        REGISTER(GameObject);
        REGISTER(Shader);
        REGISTER(GameScene);
        REGISTER(LuaScript);
        REGISTER(Texture);
        REGISTER(AssetObject);
        EditorRegister::Instance()->RegisterInspector<Rendering::RenderPipelineAsset, RenderPipelineAssetInspector>();
    }

    static void MaterialInspectorShowStructureData(Material& mat, const std::string& bindingName, Gfx::ShaderInfo::StructuredData& data)
    {
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

        // pick shader
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

        // show shader parameters
        auto& shaderInfo = shader->GetShaderProgram()->GetShaderInfo();
        int imguiID = 0;
        for(auto& b : shaderInfo.bindings)
        {
            if (b.second.setNum != Gfx::Material_Descriptor_Set) continue;
            if (b.second.type == Gfx::ShaderInfo::BindingType::UBO)
            {
                for (auto& uniMem : b.second.binding.ubo.data.members)
                {
                    std::string bindingName = b.second.name + "." + uniMem.second.name;
                    ImGui::Text("%s", bindingName.c_str());
                    switch (uniMem.second.data->type)
                    {
                        case Gfx::ShaderInfo::ShaderDataType::Half:
                        case Gfx::ShaderInfo::ShaderDataType::Float:
                            {
                                float val = mat->GetFloat(b.first, uniMem.second.name);
                                if(ImGui::DragFloat(("##" + bindingName).c_str(), &val))
                                {
                                    mat->SetFloat(b.first, uniMem.second.name, val);
                                }
                            }
                            break;
                        case Gfx::ShaderInfo::ShaderDataType::Vec2:
                            {
                                glm::vec4 vec = mat->GetVector(b.second.name, uniMem.second.name);
                                if(ImGui::DragFloat2(("##" + bindingName).c_str(), &vec.x))
                                {
                                    mat->SetVector(b.first, uniMem.second.name, vec);
                                }
                            }
                            break;
                        case Gfx::ShaderInfo::ShaderDataType::Vec3:
                            {
                                glm::vec4 vec = mat->GetVector(b.second.name, uniMem.second.name);
                                ImGui::Text("%s", bindingName.c_str());
                                ImGui::SameLine();
                                if(ImGui::DragFloat3(("##" + bindingName).c_str(), &vec.x))
                                {
                                    mat->SetVector(b.first, uniMem.second.name, vec);
                                }
                            }
                            break;
                        case Gfx::ShaderInfo::ShaderDataType::Vec4:
                            {
                                glm::vec4 vec = mat->GetVector(b.second.name, uniMem.second.name);
                                if(ImGui::DragFloat4(("##" + bindingName).c_str(), &vec.x))
                                {
                                    mat->SetVector(b.first, uniMem.second.name, vec);
                                }
                            }
                            break;
                        default:
                            ImGui::LabelText("Type Not Implemented", "%s", uniMem.second.name.c_str());
                            break;
                    }
                }
            }
            else if (b.second.type == Gfx::ShaderInfo::BindingType::Texture)
            {
                auto tex = mat->GetTexture(b.first);
                std::string label = b.first;
                if (tex) label += ":" + tex->GetName();
                ImGui::Button(label.c_str());
                if (tex)
                {
                    ImGui::SameLine();
                    if(ImGui::Button(("x##" + std::to_string(imguiID)).c_str()))
                    {
                        mat->SetTexture(b.first, nullptr);
                    }
                    float width = ImGui::GetWindowSize().x;
                    auto& desc = tex->GetDescription();
                    float ratio = desc.width / (float)desc.height;
                    ImGui::Image(tex->GetGfxImage().Get(), {width, width / ratio});
                }

                if (ImGui::BeginDragDropTarget())
                {
                    auto payload = ImGui::GetDragDropPayload();
                    if (Texture* asTexture = DYNAMIC_CAST_PAYLOAD(payload->Data, Texture))
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GameEditorDNDPayload"))
                        {
                            mat->SetTexture(b.first, asTexture);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            imguiID += 1;
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
            tsm->SetPosition(pos);
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

    void LuaScriptInspector::Tick(RefPtr<EditorContext> editorContext)
    {
        RefPtr<LuaScript> luaScript = target;
        char textArea[256];
        auto& name = luaScript->GetLuaClass();
        strncpy(textArea, name.c_str(), name.size() + 1);
        ImGui::Text("%s", "Name: ");
        ImGui::SameLine();
        if (ImGui::InputText("##LuaScriptName", textArea, 255))
        {
            luaScript->RefLuaClass(textArea);
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
                if (ImGui::Selectable("LuaScript"))
                {
                    activeObject->AddComponent<LuaScript>();
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

    void TextureInspector::Tick(RefPtr<EditorContext> editorContext)
    {
        RefPtr<Texture> tex = target;
        float width = ImGui::GetWindowSize().x;
        auto& desc = tex->GetDescription();
        float ratio = desc.width / (float)desc.height;
        ImGui::Image(tex->GetGfxImage().Get(), {width, width / ratio});
    }

    void AssetObjectInspector::Tick(RefPtr<EditorContext> editorContext)
    {
        RefPtr<AssetObject> assetObj = target;
        for (auto it : *assetObj)
        {
            if (it.isAssetObjectPtr)
            {

            }
        }
    }

    void RenderPipelineAssetInspector::Tick(RefPtr<EditorContext> editorContext)
    {
        RefPtr<Rendering::RenderPipelineAsset> asset = target;

        AssetObjectField("virtual texture", asset->virtualTexture);
    }
}
