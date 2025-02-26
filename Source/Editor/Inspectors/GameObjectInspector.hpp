#pragma once

#include "../EditorState.hpp"
#include "Core/GameObject.hpp"
#include "Core/Component/AnimationPlayer.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/GrassSurface.hpp"
#include "Core/Component/Light.hpp"
#include "Core/Component/LightFieldProbes.hpp"
#include "Core/Component/GameScript.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Component/PhysicsBody.hpp"
#include "Core/Component/SceneEnvironment.hpp"
#include "Core/Scene/Scene.hpp"
#include "GamePlay/Component/PlayerController.hpp"
#include "Inspector.hpp"
#include "Modules/VolumetricCloud/Cloud.hpp"
#include "ThirdParty/imgui/imgui.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Editor
{
class GameObjectInspector : public Inspector<GameObject>
{
public:
    void DrawInspector(GameEditor& editor) override;

private:
    void PreventNegativeZero(float& val);

private:
    static char _register;
    glm::vec3 point, axis;
    float angle;
};

} // namespace Editor
