#include "Gizmo.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Texture.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include <glm/glm.hpp>

Shader* GizmoBase::GetBillboardShader()
{
    static Shader* shader = nullptr;
    if (shader == nullptr)
        shader = static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Billboard.shad"));

    return shader;
}

GizmoLight::GizmoLight(const glm::vec3& position) : position(position) {}
void GizmoLight::Draw(Gfx::CommandBuffer& cmd)
{
    Shader* shader = GizmoBase::GetBillboardShader();

    glm::vec4 pos(position, 1.0);
    glm::vec4 pconst[2] = {pos, glm::vec4(scale, 1.0)};
    Gfx::ShaderProgram* program = shader->GetDefaultShaderProgram();
    cmd.SetTexture("mainTex", *GetLightTexture()->GetGfxImage());
    cmd.BindShaderProgram(program, shader->GetDefaultShaderConfig());
    cmd.SetPushConstant(shader->GetDefaultShaderProgram(), &pconst);
    cmd.Draw(6, 1, 0, 0);
}

Texture* GizmoLight::GetLightTexture()
{
    static Texture* lightTex = nullptr;
    if (lightTex == nullptr)
    {
        lightTex =
            static_cast<Texture*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Editor/Gizmos/Light.ktx"));
    }

    return lightTex;
}
// namespace Gizmos
