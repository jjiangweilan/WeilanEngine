#pragma once
#include "Resource.hpp"

namespace Engine::RenderGraph
{

// RenderPass should request any resource it creates using ResourceCenter so that the graph can manage these resources
class ResourceCenter
{
public:
};
} // namespace Engine::RenderGraph
