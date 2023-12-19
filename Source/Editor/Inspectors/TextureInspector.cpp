#include "../EditorState.hpp"
#include "Core/Texture.hpp"
#include "Inspector.hpp"
namespace Editor
{
class TextureInspector : public Inspector<Texture>
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

        auto width = target->GetDescription().img.width;
        auto height = target->GetDescription().img.height;
        auto contentMax = ImGui::GetWindowContentRegionMax();
        auto contentMin = ImGui::GetWindowContentRegionMin();
        auto contentWidth = contentMax.x - contentMin.y;
        auto contentHeight = contentMax.y - contentMin.y;
        auto size = ResizeKeepRatio(width, height, contentWidth, contentHeight);
        ImGui::Image(&target->GetGfxImage()->GetDefaultImageView(), {size.x, size.y});
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

const char TextureInspector::_register = InspectorRegistry::Register<TextureInspector, Texture>();

} // namespace Editor
