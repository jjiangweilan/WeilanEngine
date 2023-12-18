#include "GameLoop.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include "Rendering/RenderPipeline.hpp"
#include "Scene/RenderingScene.hpp"
#include "Scene/Scene.hpp"
#include <spdlog/spdlog.h>

namespace Engine
{
void GameLoop::Tick()
{
    RenderingScene();
}

void GameLoop::RenderScene()
{
    if (scene)
    {
        auto mainCam = scene->GetMainCamera();
        FrameGraph::Graph* frameGraph = mainCam ? mainCam->GetFrameGraph() : nullptr;
        if (frameGraph)
        {
            bool compiled = frameGraph->IsCompiled();
            if (!compiled)
            {
                compiled = frameGraph->Compile();
            }

            if (!compiled)
            {
                SPDLOG_WARN("frame graph failed to compile");
                return;
            }

            RenderPipeline::Singleton().Schedule(
                [mainCam](Gfx::CommandBuffer& cmd)
                { mainCam->GetFrameGraph()->Execute(cmd, *mainCam->GetGameObject()->GetScene()); }
            );
        }
    }
}
} // namespace Engine
