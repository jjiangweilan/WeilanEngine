#include "../EditorState.hpp"
#include "EditorGUI.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "Inspector.hpp"
#include "Rendering/EnumStringMapping.hpp"
#include "Rendering/Material.hpp"
#include "GfxDriver/ShaderProgram.hpp"
namespace Editor
{
class MaterialInspector : public Inspector<Material>
{
public:
    void OnEnable(Object& obj) override
    {
        Inspector<Material>::OnEnable(obj);

        featureToEnable[0] = '\0';

        for (auto& obj : Object::GetAllEngineObjects())
        {
            if (obj.second->GetObjectTypeID() == Shader2::StaticGetObjectTypeID())
            {
                shaders.push_back(static_cast<Shader2*>(obj.second));
            }
        }

        std::sort(shaders.begin(), shaders.end(), [](Shader2* f, Shader2* s) { return f->GetShaderProgram()->GetName() < s->GetShaderProgram()->GetName(); });
    }

    //void ShowFeatures(const std::vector<std::vector<std::string>>& features)
    //{
    //    for (auto& fs : features)
    //    {
    //        for (auto& f : fs)
    //        {
    //            if (f != ShaderBase::DefaultGlobalFeatureWord)
    //            {
    //                bool enabled = target->IsFeatureEnabled(f);
    //                ImGui::PushStyleColor(
    //                    ImGuiCol_Button,
    //                    enabled ? ImVec4(0.2, 0.7, 0.2, 1) : ImVec4(0.7, 0.2, 0.2, 1)
    //                );
    //                if (ImGui::Button(f.c_str()))
    //                {
    //                    if (enabled)
    //                        target->DisableFeature(f);
    //                    else
    //                        target->EnableFeature(f);
    //                }
    //                ImGui::PopStyleColor();
    //            }
    //        }
    //    }
    //}

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

        if (auto shader = target->GetShader())
        {
            // ShowFeatures(shader->GetDefaultShaderConfig()->vertFeatures);
            // ShowFeatures(shader->GetDefaultShaderConfig()->fragFeatures);
        }

        auto shader = target->GetShader();
        std::string shaderGUIID = "empty";

        // int selectedIndex = 0;
        // if (shader != nullptr)
        // {
        //     for (auto s : shaders)
        //     {
        //         if (shader.Get() == s)
        //             break;
        //         selectedIndex++;
        //     }
        // }
        // if (GUI::EnumDropDown(
        //         "shader",
        //         selectedIndex,
        //         shaders.size(),
        //         [this](int i) { return shaders[i]->GetShaderProgram()->GetName().c_str(); }
        //     ))
        // {
        //     target->SetShader(shaders[selectedIndex]);
        // }

        if (shader)
        {
            auto& shaderInfo = target->GetShaderProgram()->GetShaderInfo();

            std::vector<Gfx::PipelineInfo::Binding> textureBindings;
            std::vector<Gfx::PipelineInfo::Binding> numericBindings;
            /*for (auto& binding : shaderInfo.bindings)
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

            std::sort(
                numericBindings.begin(),
                numericBindings.end(),
                [](Gfx::ShaderInfo::Binding& left, Gfx::ShaderInfo::Binding& right) { return left.name < right.name; }
            );

            std::sort(
                textureBindings.begin(),
                textureBindings.end(),
                [](Gfx::ShaderInfo::Binding& left, Gfx::ShaderInfo::Binding& right) { return left.name < right.name; }
            );

            ImGui::Spacing();
            ImGui::Text("Numerics");
            ImGui::Separator();
            for (auto& numBinding : numericBindings)
            {
                ImGui::Text("%s", numBinding.name.c_str());
                std::vector<std::pair<std::string, Gfx::ShaderInfo::Member*>> members;
                for (auto& iter : numBinding.binding.ubo.data.members)
                {
                    members.push_back(std::make_pair(iter.first, &iter.second));
                }
                std::sort(members.begin(), members.end(), [](auto& l, auto& r) { return l.first < r.first; });

                for (auto& m : members)
                {
                    std::string id = fmt::format("##{}", m.first);
                    ImGui::Indent();

                    ImGui::Text("%s", m.second->name.c_str());
                    ImGui::SameLine();
                    if (m.second->data->type == Gfx::ShaderInfo::ShaderDataType::Float)
                    {
                        float val = target->GetFloat(numBinding.name, m.first);
                        if (ImGui::DragFloat(id.c_str(), &val))
                        {
                            target->SetFloat(numBinding.name, m.first, val);
                        }
                    }
                    else if (m.second->data->type == Gfx::ShaderInfo::ShaderDataType::Vec2)
                    {
                        auto val = target->GetVector(numBinding.name, m.first);
                        if (ImGui::DragFloat2(id.c_str(), &val[0]))
                        {
                            target->SetVector(numBinding.name, m.first, val);
                        }
                    }
                    else if (m.second->data->type == Gfx::ShaderInfo::ShaderDataType::Vec3)
                    {
                        auto val = target->GetVector(numBinding.name, m.first);
                        if (ImGui::DragFloat3(id.c_str(), &val[0]))
                        {
                            target->SetVector(numBinding.name, m.first, val);
                        }
                    }
                    else if (m.second->data->type == Gfx::ShaderInfo::ShaderDataType::Vec4)
                    {
                        auto val = target->GetVector(numBinding.name, m.first);
                        if (ImGui::DragFloat4(id.c_str(), &val[0]))
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
            }*/

            ImGui::Spacing();
            ImGui::Text("ShaderConfig");
            auto config = target->GetShaderConfig();
            auto json = config.ToJson();
            if(GUI::JsonInspector(json))
            {
                config = Gfx::PipelineConfig::FromJson(json);
                target->SetShaderConfig(*config);
            }
            // int cullMode = static_cast<int>(config.cullMode);
            //
            // if (GUI::EnumDropDown(
            //         "CullMode",
            //         cullMode,
            //         static_cast<int>(Gfx::CullMode::MAX_COUNT),
            //         [](int index) { return Utils::MapStrCullMode(static_cast<Gfx::CullMode>(index)); }
            //     ))
            // {
            //     config.cullMode = static_cast<Gfx::CullMode>(cullMode);
            //     target->SetShaderConfig(config);
            // }

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
                        EditorState::SelectObject(tex->GetSRef());
                    }
                }
                else
                    ImGui::Button(texBinding.name.c_str());

                Object* texturePayload;
                if (GUI::DragDropTarget(typeid(Texture), texturePayload))
                {
                    Texture* texture = (Texture*)texturePayload;
                    target->SetTexture(texBinding.name, texture);
                }
            }
        }
    }

private:
    static const char _register;
    char featureToEnable[256];
    std::vector<Shader2*> shaders;

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

} // namespace Editor
