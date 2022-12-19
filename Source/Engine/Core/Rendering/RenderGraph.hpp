#pragma once
#include "GfxDriver/RenderPass.hpp"

namespace Engine::Rendering::RenderGraph {
// GfxDriver is a thin wrapper around Graphis API. Graphics API like Vulkan,
// which is our main target Graphics API , need explicit dynamic resource
// management. RenderGraph split the rendering logics into 'compile' and
// 'execute' so that we have a chance to configure out at want point a dynamic
// resource is needed dynamic resource: any resource that need a memory
// management within the render pipeline
class Node {
public:
  virtual void AddDependency() = 0;
  virtual void Compile() = 0;
  virtual void Execute() = 0;
};

class RenderPassBeginNode : public Node {
public:
  void Compile() override;

private:
};

class RenderPassEndNode : public Node {};

class ImageNode : public Node {};

class DrawNode : public Node {};

class BlitNode : public Node {};

class SwapChainImageNode : public Node {};

class RenderGraph {
public:
public:
  RenderGraph();

private:
};
} // namespace Engine::Rendering::RenderGraph
