#pragma once
#include "Core/Model.hpp"

class Importers
{
public:
    static std::unique_ptr<Model> GLB(const char* path, Shader* shader = nullptr);
};
