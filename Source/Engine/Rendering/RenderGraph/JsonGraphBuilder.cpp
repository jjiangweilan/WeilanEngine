#include "JsonGraphBuilder.hpp"
#include <spdlog/spdlog.h>

namespace Engine::RenderGraph
{

std::unique_ptr<Graph> JsonGraphBuilder::Build()
{
    graph = std::make_unique<Graph>();

    // create declared images
    for (auto& image : j)
    {
        ImageNode* imageNode = nullptr;

        // id
        std::string id = image.value("id", "");
        if (id != "")
        {
            imageNode = graph->AddNode<ImageNode>();
            imageNodes[id] = imageNode;
        }
        else
        {
            SPDLOG_ERROR("Fail to parse json render graph: invalid image id");
            return nullptr;
        }

        // format
        Gfx::ImageFormat format = image.value("format", Gfx::ImageFormat::Invalid);
        if (format != Gfx::ImageFormat::Invalid)
        {
            imageNode->format = format;
        }
        else
        {
            SPDLOG_ERROR("Fail to parse json render graph: invalid image format");
            return nullptr;
        }

        // size
        std::vector<int> size = image.value("size", std::vector<int>{0, 0, 0});
        if (size.size() >= 2)
        {
            imageNode->size.x = size[0];
            imageNode->size.y = size[1];
        }
        if (size.size() >= 3)
        {
            imageNode->size.z = size[3];
        }
        else
        {
            SPDLOG_ERROR("Fail to parse json render graph: invalid image size");
            return nullptr;
        }
    }

    return std::move(graph);
}
} // namespace Engine::RenderGraph
