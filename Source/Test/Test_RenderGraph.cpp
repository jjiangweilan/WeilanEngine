#pragma once
#include "Core/Rendering/RenderGraph/RenderGraph.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

using namespace Engine::RGraph;
TEST(RenderGraph, Test0)
{
    Engine::RGraph::RenderGraph renderGraph;
    auto color = renderGraph.AddNode<ImageNode>();
    auto depth = renderGraph.AddNode<ImageNode>();

    color->width = 1024;
    color->height = 1024;
    color->format = Engine::Gfx::ImageFormat::B8G8R8A8_UNorm;

    depth->width = color->width;
    depth->height = color->height;
    depth->format = Engine::Gfx::ImageFormat::D24_UNorm_S8_UInt;

    auto rpBegin = renderGraph.AddNode<RenderPassBeginNode>();
    auto rpEnd = renderGraph.AddNode<RenderPassEndNode>();

    // color->GetPortOutput()->Connect(rpBegin->GetPortColor(0));
    // depth->GetPortOutput()->Connect(rpBegin->GetPortDepth());

    // auto drawNode = renderGraph.AddNode<DrawNode>();
}
