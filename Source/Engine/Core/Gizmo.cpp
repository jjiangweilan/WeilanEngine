#include "Gizmo.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Texture.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Rendering/Shader.hpp"
#include <glm/glm.hpp>

Shader* GizmoBase::GetBillboardShader()
{
    static Shader* shader = nullptr;
    if (shader == nullptr)
        shader = static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Billboard.shad"));

    return shader;
}

class GizmoDrawLight : public GizmoBase
{
public:
    GizmoDrawLight() : position(0){};
    GizmoDrawLight(const glm::vec3& position);
    void Draw(Gfx::CommandBuffer& cmd) override
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

    AABB GetAABB() override
    {
        return AABB(position, scale);
    }

private:
    glm::vec3 position;
    const glm::vec3 scale = glm::vec3(0.7f);
    static Texture* GetLightTexture()
    {
        static Texture* lightTex = nullptr;
        if (lightTex == nullptr)
        {
            lightTex =
                static_cast<Texture*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Editor/Gizmos/Light.ktx")
                );
        }

        return lightTex;
    }
};

class GizmoDrawMesh : public GizmoBase
{
public:
    GizmoDrawMesh(Mesh* mesh, int submeshIndex, Shader* shader, const glm::mat4& modelMatrix)
        : mesh(mesh), submeshIndex(submeshIndex), shader(shader), material(nullptr), modelMatrix(modelMatrix)
    {}

    GizmoDrawMesh(Mesh* mesh, int submeshIndex, Material* material, const glm::mat4& modelMatrix)
        : mesh(mesh), submeshIndex(submeshIndex), shader(nullptr), material(material), modelMatrix(modelMatrix)
    {}

    void Draw(Gfx::CommandBuffer& cmd) override
    {
        auto& submeshes = mesh->GetSubmeshes();
        if (submeshIndex < submeshes.size())
        {
            auto& submesh = submeshes[submeshIndex];
            cmd.BindIndexBuffer(submesh.GetIndexBuffer(), 0, submesh.GetIndexBufferType());
            cmd.BindVertexBuffer(submesh.GetGfxVertexBufferBindings(), 0);
            if (material != nullptr)
            {
                Gfx::ShaderProgram* program = material->GetShaderProgram();
                cmd.BindResource(2, material->GetShaderResource());
                cmd.SetPushConstant(program, &modelMatrix);
                cmd.BindShaderProgram(program, program->GetDefaultShaderConfig());
            }
            else
            {
                cmd.SetPushConstant(shader->GetDefaultShaderProgram(), &modelMatrix);
                cmd.BindShaderProgram(shader->GetDefaultShaderProgram(), shader->GetDefaultShaderConfig());
            }
            cmd.DrawIndexed(submesh.GetIndexCount(), 1, 0, 0, 0);
        }
    }
    AABB GetAABB() override
    {
        auto& submeshes = mesh->GetSubmeshes();
        if (submeshIndex < submeshes.size())
        {
            auto aabb = submeshes[submeshIndex].GetAABB();
            aabb.max += glm::vec3(modelMatrix[3]);
            aabb.min += glm::vec3(modelMatrix[3]);
            return aabb;
        }
        else
            return AABB(glm::vec3(0), glm::vec3(0));
    }

private:
    Mesh* mesh;
    int submeshIndex;
    Shader* shader;
    Material* material;
    glm::mat4 modelMatrix;
};

class GizmoCamera : public GizmoBase
{
public:
    GizmoCamera(){};
    GizmoCamera(float fov, float near, float far, float aspect);

private:
    float fov;
    float near;
    float far;
    float aspect;
};

GizmoDrawLight::GizmoDrawLight(const glm::vec3& position) : position(position) {}

Gizmos& Gizmos::GetSingleton()
{
    static Gizmos gizmos;
    return gizmos;
}

bool Gizmos::RayVsAABB(const Ray& r, const AABB& aabb, float& t)
{
    glm::vec3 lb = aabb.min;
    glm::vec3 rt = aabb.max;
    glm::vec3 dirfrac;
    // r.dir is unit direction vector of ray
    dirfrac.x = 1.0f / r.direction.x;
    dirfrac.y = 1.0f / r.direction.y;
    dirfrac.z = 1.0f / r.direction.z;
    // lb is the corner of AABB with minimal coordinates - left bottom, rt is maximal corner
    // r.org is origin of ray
    float t1 = (lb.x - r.origin.x) * dirfrac.x;
    float t2 = (rt.x - r.origin.x) * dirfrac.x;
    float t3 = (lb.y - r.origin.y) * dirfrac.y;
    float t4 = (rt.y - r.origin.y) * dirfrac.y;
    float t5 = (lb.z - r.origin.z) * dirfrac.z;
    float t6 = (rt.z - r.origin.z) * dirfrac.z;

    float tmin = glm::max(glm::max(glm::min(t1, t2), glm::min(t3, t4)), glm::min(t5, t6));
    float tmax = glm::min(glm::min(glm::max(t1, t2), glm::max(t3, t4)), glm::max(t5, t6));

    // if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
    if (tmax < 0)
    {
        t = tmax;
        return false;
    }

    // if tmin > tmax, ray doesn't intersect AABB
    if (tmin > tmax)
    {
        t = tmax;
        return false;
    }

    t = tmin;
    return true;
}

void Gizmos::PickGizmos(const Ray& ray, std::vector<GameObject*>& result)
{
    result.clear();
    for (auto& g : GetSingleton().gizmos)
    {
        AABB aabb = g->GetAABB();
        float t;
        if (RayVsAABB(ray, aabb, t))
        {
            if (GameObject* carrier = g->GetCarrier())
                result.push_back(carrier);
        }
    }
}

void Gizmos::DispatchAllDiszmos(Gfx::CommandBuffer& cmd)
{
    for (auto& g : GetSingleton().gizmos)
    {
        g->Draw(cmd);
    }
}

GameObject*& GizmoBase::GetActiveCarrier()
{
    static GameObject* activeCarrier;
    return activeCarrier;
}

void GizmoBase::SetActiveCarrier(GameObject* carrier)
{
    GetActiveCarrier() = carrier;
}

void GizmoBase::ClearActiveCarrier()
{
    GetActiveCarrier() = nullptr;
}

void Gizmos::DrawLight(const glm::vec3& position)
{
    GetSingleton().gizmos.push_back(std::make_unique<GizmoDrawLight>(position));
}

void Gizmos::DrawMesh(Mesh& mesh, int submeshIndex, Shader* shader, const glm::mat4& modelMatrix)
{
    GetSingleton().gizmos.push_back(std::make_unique<GizmoDrawMesh>(&mesh, submeshIndex, shader, modelMatrix));
}

void Gizmos::DrawMesh(Mesh& mesh, int submeshIndex, Material* material, const glm::mat4& modelMatrix)
{
    GetSingleton().gizmos.push_back(std::make_unique<GizmoDrawMesh>(&mesh, submeshIndex, material, modelMatrix));
}

// namespace Gizmos
