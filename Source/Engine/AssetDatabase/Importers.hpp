#pragma once
#include "Asset/Shader.hpp"
#include "Core/Model.hpp"

class Importers
{
public:
    static std::unique_ptr<Model> GLB(const char* path, Shader* shader = nullptr);
};
