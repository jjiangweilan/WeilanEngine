#include "../EditorState.hpp"
#include "Asset/Material.hpp"
#include "Inspector.hpp"
namespace Engine::Editor
{
class MaterialInspector : public Inspector<Material>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        // object information
        auto& name = target->GetName();
        char cname[1024];
        strcpy(cname, name.data());
        if (ImGui::InputText("Name", cname, 1024))
        {
            target->SetName(cname);
        }

        auto shader = target->GetShader();

        std::string shaderGUIID = "empty";

        if (shader != nullptr)
            shaderGUIID = shader->GetName();

        bool buttonPressed = ImGui::Button(fmt::format("{}##shader", shaderGUIID).c_str());

        if (ImGui::BeginDragDropTarget())
        {
            auto payload = ImGui::AcceptDragDropPayload("object");
            if (payload && payload->IsDelivery())
            {
                Object& obj = **(Object**)payload->Data;
                if (typeid(obj) == typeid(Shader))
                {
                    Shader* sourceShader = static_cast<Shader*>(&obj);
                    target->SetShader(sourceShader);
                }
            }
            ImGui::EndDragDropTarget();
        }
        else if (buttonPressed)
        {
            if (shader != nullptr)
            {
                EditorState::selectedObject = shader;
            }
        }

        if (shader)
        {
            auto& shaderInfo = shader->GetShaderProgram()->GetShaderInfo();

            std::vector<Gfx::ShaderInfo::Binding> textureBindings;
            std::vector<Gfx::ShaderInfo::Binding> numericBindings;
            for (auto& binding : shaderInfo.bindings)
            {
                if (binding.second.type == Gfx::ShaderInfo::BindingType::Texture &&
                    binding.second.setNum == (int)Gfx::ShaderResourceFrequency::Material)
                {
                    textureBindings.push_back(binding.second);
                }

                if (binding.second.type == Gfx::ShaderInfo::BindingType::UBO &&
                    binding.second.setNum == (int)Gfx::ShaderResourceFrequency::Material)
                {
                    numericBindings.push_back(binding.second);
                }
            }

            ImGui::Spacing();
            ImGui::Text("Numerics");
            ImGui::Separator();
            for (auto& numBinding : numericBindings)
            {
                ImGui::Text("%s", numBinding.name.c_str());
                for (auto& m : numBinding.binding.ubo.data.members)
                {
                    std::string id = fmt::format("##{}", m.first);
                    ImGui::Indent();

                    ImGui::Text("%s", m.second.name.c_str());
                    ImGui::SameLine();
                    if (m.second.data->type == Gfx::ShaderInfo::ShaderDataType::Float)
                    {
                        float val = target->GetFloat(numBinding.name, m.first);
                        if (ImGui::InputFloat(id.c_str(), &val))
                        {
                            target->SetFloat(numBinding.name, m.first, val);
                        }
                    }
                    else if (m.second.data->type == Gfx::ShaderInfo::ShaderDataType::Vec2)
                    {
                        auto val = target->GetVector(numBinding.name, m.first);
                        if (ImGui::InputFloat2(id.c_str(), &val[0]))
                        {
                            target->SetVector(numBinding.name, m.first, val);
                        }
                    }
                    else if (m.second.data->type == Gfx::ShaderInfo::ShaderDataType::Vec3)
                    {
                        auto val = target->GetVector(numBinding.name, m.first);
                        if (ImGui::InputFloat3(id.c_str(), &val[0]))
                        {
                            target->SetVector(numBinding.name, m.first, val);
                        }
                    }
                    else if (m.second.data->type == Gfx::ShaderInfo::ShaderDataType::Vec4)
                    {
                        auto val = target->GetVector(numBinding.name, m.first);
                        if (ImGui::InputFloat4(id.c_str(), &val[0]))
                        {
                            target->SetVector(numBinding.name, m.first, val);
                        }
                    }
                    else
                    {
                        ImGui::Text("%s", m.first.c_str());
                    }

                    ImGui::Unindent();
                }
            }

            ImGui::Spacing();
            ImGui::Text("Textures");
            ImGui::Separator();
            for (auto& texBinding : textureBindings)
            {
                Texture* tex = target->GetTexture(texBinding.name);
                if (tex)
                {
                    auto width = tex->GetDescription().img.width;
                    auto height = tex->GetDescription().img.height;
                    auto size = ResizeKeepRatio(width, height, 35, 35);
                    ImGui::Text("%s", texBinding.name.c_str());
                    ImGui::SameLine();
                    ImGui::Image(&tex->GetGfxImage()->GetDefaultImageView(), {size.x, size.y});

                    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    {
                        EditorState::selectedObject = tex;
                    }
                }
                else
                    ImGui::Button(texBinding.name.c_str());

                if (ImGui::BeginDragDropTarget())
                {
                    auto payload = ImGui::AcceptDragDropPayload("object");
                    if (payload && payload->IsDelivery())
                    {
                        Object* obj = *(Object**)payload->Data;
                        if (Texture* dropTex = dynamic_cast<Texture*>(obj))
                        {
                            target->SetTexture(texBinding.name, dropTex);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        }
    }

private:
    static const char _register;

    glm::vec2 ResizeKeepRatio(float width, float height, float contentWidth, float contentHeight)
    {
        float imageWidth = width;
        float imageHeight = height;

        // shrink width
        if (imageWidth > contentWidth)
        {
            float ratio = contentWidth / (float)imageWidth;
            imageWidth = contentWidth;
            imageHeight *= ratio;
        }

        if (imageHeight > contentHeight)
        {
            float ratio = contentHeight / (float)imageHeight;
            imageHeight = contentHeight;
            imageWidth *= ratio;
        }

        return {imageWidth, imageHeight};
    }
};

const char MaterialInspector::_register = InspectorRegistry::Register<MaterialInspector, Material>();

} // namespace Engine::Editor
