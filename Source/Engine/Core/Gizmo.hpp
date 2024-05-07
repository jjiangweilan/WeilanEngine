#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <variant>
#include <vector>

class Texture;
class Shader;
namespace Gfx
{
class CommandBuffer;
}

class GizmoBase
{
public:
    static Shader* GetBillboardShader();
};

class GizmoDirectionalLight final : public GizmoBase
{

public:
    GizmoDirectionalLight(glm::vec3 position);
    void Draw(Gfx::CommandBuffer& cmd);

private:
    glm::vec3 position;
    static Texture* GetDirectionalLightTexture();
};
// namespace Gizmos

using GizmoVariant = std::variant<GizmoDirectionalLight>;

class Gizmos
{
public:
    template <std::derived_from<GizmoBase> T, class... Args>
    void Add(Args&&... args)
    {
        gizmos.push_back(T(std::forward(args)...));
    }

    void Draw(Gfx::CommandBuffer& cmd)
    {
        for (auto& g : gizmos)
        {
            std::visit([&cmd](auto&& val) { val.Draw(cmd); }, g);
        }
    }

    void Clear()
    {
        gizmos.clear();
    }

private:
    std::vector<GizmoVariant> gizmos;
};
