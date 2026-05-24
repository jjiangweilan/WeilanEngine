#pragma once
#include "./NavData.hpp"

#include "Engine/Runtime/Object/Graphics/Mesh.hpp"

class NavDataBaker
{
public:
    void Bake(Mesh* mesh, NavData& navData);

private:
};
