// #pragma once
// #include "../Tool.hpp"
// #include "Core/Model.hpp"
// #include "Core/Scene/Scene.hpp"
// #include "Rendering/RenderGraph/Graph.hpp"
//
// namespace Editor
//{
//
// class EnvironmentBaker : public Tool
//{
// public:
//     EnvironmentBaker();
//     ~EnvironmentBaker() override{};
//
//     void Open() override;
//     void Close() override;
//
// public:
//     std::vector<std::string> GetToolMenuItem() override
//     {
//         return {"Tools", "Scene", "Environment Baker"};
//     }
//
//     bool Tick() override;
//
// private:
//     struct ShaderParamBakeInfo
//     {
//         glm::vec4 uFrom, uTo;
//         glm::vec4 vFrom, vTo;
//         float roughness;
//     };
//     std::unique_ptr<Gfx::Image> sceneImage;
//     std::unique_ptr<RenderGraph::Graph> renderGraph;
//     std::unique_ptr<Shader> previewShader;
//     std::unique_ptr<Model> model;
//     int size = 512;
//     float roughness = 0.1;
//
//     // these two baker shares the same resource
//     std::unique_ptr<Gfx::ShaderProgram> lightingBaker;
//     std::unique_ptr<Gfx::ShaderProgram> brdfBaker;
//     std::unique_ptr<Gfx::ShaderResource> bakingShaderResource;
//     std::unique_ptr<Gfx::Buffer> bakeInfoBuffer;
//     std::unique_ptr<Texture> environmentMap;
//     std::unique_ptr<Gfx::Image> cubemap;
//
//     std::vector<std::unique_ptr<Gfx::ImageView>> viewResults;
//
//     void CreateRenderData(uint32_t width, uint32_t height);
//     void Bake(int size);
//     void BakeToCubeFace(Gfx::Image& cubemap, uint32_t layer, ShaderParamBakeInfo bakeInfo);
//     void Render(Gfx::CommandBuffer& cmd) override;
// };
// } // namespace Editor
