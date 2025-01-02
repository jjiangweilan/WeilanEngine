#pragma once
#include "Core/Component/Component.hpp"
#include "Libs/Math.hpp"

class Scene;

namespace Rendering::GI
{
struct BakeProbesInfo
{
    glm::float3 gridMin;
    glm::float3 gridMax;
    glm::float3 probeCount;
};
class GlobalIlluminaion
{
public:
    void PreprocessScene(Scene& scene, const BakeProbesInfo& info);

public:
};

class GIScene : public Component
{
public:
    DECLARE_OBJECT();

    GIScene();
    GIScene(GameObject* gameObject);
    ~GIScene();

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
};
} // namespace Rendering::GI
