#include "Gizmo.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Texture.hpp"
#include "Editor/Gizmos/MeshGizmo.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Rendering/Graphics.hpp"
#include "Rendering/Shader.hpp"
#include <glm/glm.hpp>

ObjPtr<Shader2> GizmoBase::GetBillboardShader()
{
    static ObjPtr<Shader2> shader = nullptr;
    if (shader == nullptr)
        shader = ShaderLibrary::GetShader("Billboard");

    return shader;
}

class GizmoDrawLight : public GizmoBase
{
public:
    GizmoDrawLight() : position(0) {};
    GizmoDrawLight(const glm::vec3& position) : position(position) {}
    void Draw(Gfx::CommandBuffer& cmd) override
    {
        ObjPtr<Shader2> shader = GizmoBase::GetBillboardShader();

        glm::vec4 pos(position, 1.0);
        glm::vec4 pconst[2] = {pos, glm::vec4(scale, 1.0)};
        Gfx::ShaderProgram* program = shader->GetShaderProgram();
        cmd.BindResource(0, perScene);
        cmd.BindResource(
            GetMaterial()->GetSet(Gfx::DescriptorSetSemantics::Material),
            GetMaterial()->GetShaderResource()
        );
        cmd.SetPushConstant(shader->GetShaderProgram(), &pconst);
        cmd.BindShaderProgram(program, shader->GetShaderProgram()->GetDefaultShaderConfig());
        cmd.Draw(6, 1, 0, 0);
    }

    bool Pick(const Ray& ray) override
    {
        float t;
        return RayVsAABB(ray, GetAABB(), t) && t > 0;
    }

private:
    glm::vec3 position;
    const glm::vec3 scale = glm::vec3(0.7f);
    AABB GetAABB() { return AABB(position, scale, AABB::PosConstruct{}); }

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
    static std::unique_ptr<Material>& GetMaterial()
    {
        static std::unique_ptr<Material> mat;
        if (mat == nullptr)
        {
            mat = std::make_unique<Material>();
            mat->SetShader(GetBillboardShader());
            mat->SetTexture("mainTex", GetLightTexture());
        }

        return mat;
    }

    friend class Gizmos;
};

class GizmoDrawCamera : public GizmoBase
{
public:
    GizmoDrawCamera() : position(0) {};
    GizmoDrawCamera(const glm::vec3& position) : position(position) {}
    void Draw(Gfx::CommandBuffer& cmd) override
    {
        ObjPtr<Shader2> shader = GizmoBase::GetBillboardShader();

        glm::vec4 pos(position, 1.0);
        glm::vec4 pconst[2] = {pos, glm::vec4(scale, 1.0)};
        Gfx::ShaderProgram* program = shader->GetShaderProgram();
        cmd.BindResource(0, perScene);
        cmd.BindResource(
            GetMaterial()->GetSet(Gfx::DescriptorSetSemantics::Material),
            GetMaterial()->GetShaderResource()
        );
        cmd.SetPushConstant(shader->GetShaderProgram(), &pconst);
        cmd.BindShaderProgram(program, shader->GetShaderProgram()->GetDefaultShaderConfig());
        cmd.Draw(6, 1, 0, 0);
    }

    bool Pick(const Ray& ray) override
    {
        float t;
        return RayVsAABB(ray, GetAABB(), t) && t > 0;
    }

private:
    glm::vec3 position;
    const glm::vec3 scale = glm::vec3(0.7f);
    AABB GetAABB() { return AABB(position, scale, AABB::PosConstruct{}); }

    static Texture* GetCameraIcon()
    {
        static Texture* lightTex = nullptr;
        if (lightTex == nullptr)
        {
            lightTex =
                static_cast<Texture*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Editor/Gizmos/camera.png")
                );
        }

        return lightTex;
    }
    static std::unique_ptr<Material>& GetMaterial()
    {
        static std::unique_ptr<Material> mat;
        if (mat == nullptr)
        {
            mat = std::make_unique<Material>();
            mat->SetShader(GetBillboardShader());
            mat->SetTexture("mainTex", GetCameraIcon());
        }

        return mat;
    }

    friend class Gizmos;
};

class GizmoDrawIcon : public GizmoBase
{
public:
    GizmoDrawIcon() : position(0) {};
    GizmoDrawIcon(Texture* icon, const glm::vec3& position) {};
    void Draw(Gfx::CommandBuffer& cmd) override
    {
        ObjPtr<Shader2> shader = GizmoBase::GetBillboardShader();

        glm::vec4 pos(position, 1.0);
        glm::vec4 pconst[2] = {pos, glm::vec4(scale, 1.0)};
        Gfx::ShaderProgram* program = shader->GetShaderProgram();
        cmd.BindResource(0, perScene);
        cmd.BindResource(
            GetMaterial()->GetSet(Gfx::DescriptorSetSemantics::Material),
            GetMaterial()->GetShaderResource()
        );
        cmd.SetPushConstant(shader->GetShaderProgram(), &pconst);
        cmd.BindShaderProgram(program, shader->GetShaderProgram()->GetDefaultShaderConfig());
        cmd.Draw(6, 1, 0, 0);
    }

    bool Pick(const Ray& ray) override
    {
        float t;
        return RayVsAABB(ray, GetAABB(), t) && t > 0;
    }

private:
    Texture* icon;
    glm::vec3 position;
    const glm::vec3 scale = glm::vec3(0.7f);
    AABB GetAABB() { return AABB(position, scale, AABB::PosConstruct{}); }
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

    static std::unique_ptr<Material>& GetMaterial()
    {
        static std::unique_ptr<Material> mat;
        if (mat == nullptr)
        {
            mat = std::make_unique<Material>();
            mat->SetShader(GetBillboardShader());
            mat->SetTexture("mainTex", GetLightTexture());
        }

        return mat;
    }

    friend class Gizmos;
};

class GizmoDrawInteractiveBox : public GizmoBase
{
public:
    GizmoDrawInteractiveBox(const float3& position, const float3& size) : position(position), size(size) {}
    virtual void Draw(Gfx::CommandBuffer& cmd) {};

private:
    float3 position;
    float3 size;
};

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
        if (g->Pick(ray))
        {
            if (GameObject* carrier = g->GetCarrier())
                result.push_back(carrier);
        }
    }
}

void Gizmos::DispatchAllDiszmos(Gfx::CommandBuffer& cmd, Gfx::ShaderResource* perScene)
{
    for (auto& g : GetSingleton().gizmos)
    {
        g->SetupDraw(perScene);
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

GizmoBase* Gizmos::DrawLight(const glm::vec3& position)
{
    auto g = std::make_unique<GizmoDrawLight>(position);
    auto t = g.get();
    GetSingleton().gizmos.push_back(std::move(g));
    return t;
}

GizmoBase* Gizmos::DrawCamera(const glm::vec3& position)
{
    auto g = std::make_unique<GizmoDrawCamera>(position);
    auto t = g.get();
    GetSingleton().gizmos.push_back(std::move(g));
    return t;
}

GizmoBase* Gizmos::DrawMesh(Mesh& mesh, int submeshIndex, ObjPtr<Shader2> shader, const glm::mat4& modelMatrix)
{
    auto g = std::make_unique<GizmoDrawMesh>(&mesh, submeshIndex, shader, modelMatrix);
    auto t = g.get();
    GetSingleton().gizmos.push_back(std::move(g));
    return t;
}

GizmoBase* Gizmos::DrawMesh(Mesh& mesh, int submeshIndex, Material* material, const glm::mat4& modelMatrix)
{
    auto g = std::make_unique<GizmoDrawMesh>(&mesh, submeshIndex, material, modelMatrix);
    auto t = g.get();
    GetSingleton().gizmos.push_back(std::move(g));
    return t;
}

GizmoBase* Gizmos::DrawInteractiveBox(InteractiveBox& box, const float3& position, float3& size)
{
    auto g = std::make_unique<GizmoDrawInteractiveBox>(position, size);
    auto t = g.get();
    GetSingleton().gizmos.push_back(std::move(g));
    return t;
}

void Gizmos::ResourceCleanup()
{
    GizmoDrawLight::GetMaterial() = nullptr;
}

// namespace Gizmos
