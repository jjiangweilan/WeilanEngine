#include "../EditorState.hpp"
#include "Asset/Material.hpp"
#include "Inspector.hpp"
namespace Engine::Editor
{
class MaterialInspector : public Inspector<Material>
{
public:
    void DrawInspector() override
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
                EditorState::selectedObject = shader.Get();
            }
        }

        auto& shaderInfo = target->GetShader()->GetShaderProgram()->GetShaderInfo();

        std::vector<Gfx::ShaderInfo::Binding> textureBindings;
        for (auto& binding : shaderInfo.bindings)
        {
            if (binding.second.type == Gfx::ShaderInfo::BindingType::Texture && binding.second.setNum == (int)Gfx::ShaderResourceFrequency::Material)
            {
                textureBindings.push_back(binding.second);
            }
        }
        for (auto& texBinding : textureBindings)
        {
            ImGui::Text("%s", texBinding.name.c_str());
            Texture* tex = target->GetTexture(texBinding.name);
            if (tex)
            {
                auto width = tex->GetDescription().img.width;
                auto height = tex->GetDescription().img.height;
                auto size = ResizeKeepRatio(width, height, 30, 30);
                ImGui::Image(&tex->GetGfxImage()->GetDefaultImageView(), { size.x, size.y });
            }
            else
                ImGui::Button("empty");

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
