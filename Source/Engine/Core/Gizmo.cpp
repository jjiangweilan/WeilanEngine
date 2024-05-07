#include "Gizmo.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Texture.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include <glm/glm.hpp>

GizmoDirectionalLight::GizmoDirectionalLight(glm::vec3 position) : position(position) {}
void GizmoDirectionalLight::Draw(Gfx::CommandBuffer& cmd)
{
    Shader* shader = GizmoBase::GetBillboardShader();

    glm::vec4 pos(position, 1.0);
    Gfx::ShaderProgram* program = shader->GetDefaultShaderProgram();
    cmd.BindShaderProgram(program, program->GetDefaultShaderConfig());
    cmd.SetPushConstant(shader->GetDefaultShaderProgram(), &pos);
    cmd.SetTexture("mainTex", *GetDirectionalLightTexture()->GetGfxImage());
    cmd.Draw(6, 1, 0, 0);
}

Texture* GizmoDirectionalLight::GetDirectionalLightTexture()
{
    static Texture* directionalLightTex = nullptr;
    if (directionalLightTex == nullptr)
    {
        directionalLightTex =
            static_cast<Texture*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Editor/Gizmos/Light.ktx"));
    }

    return directionalLightTex;
}
// namespace Gizmos
