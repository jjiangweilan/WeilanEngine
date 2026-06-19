#pragma once
#include "./NavData.hpp"

#include <span>

class MeshRenderer;

class NavDataBaker
{
public:
    void Bake(std::span<MeshRenderer*> renderers, NavData& navData);

private:
};
