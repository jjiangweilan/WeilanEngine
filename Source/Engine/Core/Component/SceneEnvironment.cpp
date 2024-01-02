#include "SceneEnvironment.hpp"

DEFINE_OBJECT(SceneEnvironment, "7E237F73-396E-455F-B227-657432A66877");

const std::string& SceneEnvironment::GetName()
{
    static std::string name = "SceneEnvironment";
    return name;
}

void SceneEnvironment::EnableImple() {}
void SceneEnvironment::DisableImple() {}
