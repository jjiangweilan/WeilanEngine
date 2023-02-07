#pragma once
#include "Core/Texture.hpp"
#include <cstddef>

namespace Libs::Image
{
UniPtr<Engine::Texture> LoadTextureFromBinary(unsigned char* byteData, std::size_t byteSize);
} // namespace Libs::Image
