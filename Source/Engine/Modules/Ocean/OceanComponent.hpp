#pragma once
#include "Core/Component/RenderingComponent.hpp"

#include "Core/Ptr.hpp"
#include "Rendering/Shader2.hpp"

class OceanComponent : public RenderingComponent<OceanComponent>
{
    DECLARE_RENDERING_COMPONENT(OceanComponent);
    void Serialize(Serializer* ser) const override;
    void Deserialize(Serializer* ser) override;

    ObjPtr<Mesh> plane;
    ObjPtr<Shader2> oceanShader;

public:
    void OnInit() override;

private:
};
