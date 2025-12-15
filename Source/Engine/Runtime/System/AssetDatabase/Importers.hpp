#pragma once
#include "Runtime/Object/Mesh/Model.hpp"

class Importers
{
public:
    static std::unique_ptr<Model> GLB(const char* path, Shader* shader = nullptr);
};
