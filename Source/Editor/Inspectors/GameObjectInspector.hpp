#pragma once

#include "Editor/EditorState.hpp"
#include "Runtime/Object/Component/AnimationPlayer.hpp"
#include "Runtime/Object/Component/Camera.hpp"
#include "Runtime/Object/Component/GameScript.hpp"
#include "Runtime/Object/Component/GrassSurface.hpp"
#include "Runtime/Object/Component/Light.hpp"
#include "Runtime/Object/Component/LightFieldProbes.hpp"
#include "Runtime/Object/Component/MeshRenderer.hpp"
#include "Runtime/Object/Component/PhysicsBody.hpp"
#include "Runtime/Object/Component/SceneEnvironment.hpp"
#include "Runtime/Object/GameObject/GameObject.hpp"
#include "Runtime/System/SceneManager/Scene.hpp"
#include "Game/Component/PlayerController.hpp"
#include "Inspector.hpp"
#include "Runtime/Module/VolumetricCloud/Cloud.hpp"
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
    Component* contextComponent = nullptr;
    int contextComponentIdx = -1;
    std::string searchComponent = "";
};

} // namespace Editor
