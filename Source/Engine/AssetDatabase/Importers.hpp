#pragma once
#include "Core/Graphics/Shader.hpp"
#include "Core/Model2.hpp"

namespace Engine
{
class Importers
{
public:
    static std::unique_ptr<Model2> GLB(const char* path, Shader* shader);
};
} // namespace Engine
