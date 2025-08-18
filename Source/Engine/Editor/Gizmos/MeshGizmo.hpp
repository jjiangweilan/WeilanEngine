#include "./GizmoBase.hpp"

class GizmoDrawMesh : public GizmoBase
{
public:
    GizmoDrawMesh() {}

    GizmoDrawMesh(Mesh* mesh, int submeshIndex, ObjPtr<Shader2> shader, const glm::mat4& modelMatrix)
        : mesh(mesh), submeshIndex(submeshIndex), shader(shader), material(nullptr), modelMatrix(modelMatrix)
    {}

    GizmoDrawMesh(Mesh* mesh, int submeshIndex, Material* material, const glm::mat4& modelMatrix)
        : mesh(mesh), submeshIndex(submeshIndex), shader(nullptr), material(material), modelMatrix(modelMatrix)
    {}
    void Draw(Gfx::CommandBuffer& cmd) override;
    bool Pick(const Ray& ray) override;

    void ProcessUserInput(Mesh* mesh, int submeshIndex, ObjPtr<Shader2> shader, const glm::mat4& modelMatrix)
    {
        this->mesh = mesh;
        this->submeshIndex = submeshIndex;
        this->shader = shader;
        this->material = nullptr;
        this->modelMatrix = modelMatrix;
    }
    void ProcessUserInput(Mesh* mesh, int submeshIndex, Material* material, const glm::mat4& modelMatrix)
    {
        this->mesh = mesh;
        this->submeshIndex = submeshIndex;
        this->shader = nullptr;
        this->material = material;
        this->modelMatrix = modelMatrix;
    }

private:
    Mesh* mesh;
    int submeshIndex;
    ObjPtr<Shader2> shader;
    Material* material;
    glm::mat4 modelMatrix;

    AABB GetAABB();
};
