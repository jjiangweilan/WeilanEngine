#pragma once
#include "Asset/Shader.hpp"
#include "Core/Model2.hpp"

namespace Engine
{
class Importers
{
public:
    static std::unique_ptr<Model2> GLB(const char* path, Shader* shader = nullptr);
};
} // namespace Engine
