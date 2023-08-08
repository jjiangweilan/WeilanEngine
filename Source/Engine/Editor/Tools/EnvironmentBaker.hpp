#pragma once
#include "../Tool.hpp"
#include "Core/Model2.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/RenderGraph/Graph.hpp"

namespace Engine::Editor
{

class EnvironmentBaker : public Tool
{
public:
    EnvironmentBaker();
    ~EnvironmentBaker() override{};

    void Open() override;
    void Close() override;

public:
    std::vector<std::string> GetToolMenuItem() override
    {
        return {"Tools", "Scene", "Environment Baker"};
    }

    bool Tick() override;

private:
    std::unique_ptr<Gfx::Image> sceneImage;
    std::unique_ptr<RenderGraph::Graph> renderGraph;
    std::unique_ptr<Shader> previewShader;
    std::unique_ptr<Model2> model;
    glm::ivec2 size = {1024, 1024};

    void CreateRenderData(uint32_t width, uint32_t height);
    void Bake();
};
} // namespace Engine::Editor
