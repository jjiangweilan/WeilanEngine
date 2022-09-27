#pragma once

#include "Core/GameScene/GameScene.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Window/SceneTreeWindow.hpp"
#include "Window/InspectorWindow.hpp"
#include "Window/ProjectWindow.hpp"
#include "Window/AssetExplorer.hpp"
#include "Window/GameSceneWindow.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/GameObject.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "Core/Rendering/RenderPipeline.hpp"
#include "ProjectManagement/ProjectManagement.hpp"
#include "imgui/imgui.h"
#include <SDL2/SDL.h>
#include "Code/Ptr.hpp"
#include <memory>
namespace Engine::Editor
{
    class GameEditor
    {
        public:
            GameEditor(RefPtr<Gfx::GfxDriver> gfxDriver);
            ~GameEditor();

            void ConfigEditorPath();
            void LoadCurrentProject();
            void Init(RefPtr<Rendering::RenderPipeline> renderPipeline);
            void ProcessEvent(const SDL_Event& event);
            void Tick();
            void Render();
        private:
            /* Data */
            UniPtr<ProjectManagement> projectManagement;

            /* Windows */
            UniPtr<EditorContext> editorContext;
            UniPtr<SceneTreeWindow> sceneTreeWindow;
            UniPtr<ProjectWindow> projectWindow;
            UniPtr<AssetExplorer> assetExplorer;
            UniPtr<InspectorWindow> inspector;
            UniPtr<GameSceneWindow> gameSceneWindow;

            /* Rendering Structures */
            RefPtr<Gfx::GfxDriver> gfxDriver;
            UniPtr<Gfx::RenderPass> editorPass;
            UniPtr<Gfx::FrameBuffer> frameBuffer;
            RefPtr<RenderContext> renderContext;
            RefPtr<Gfx::Image> colorImage;
            UniPtr<Gfx::RenderPass> scenePass;
            UniPtr<Gfx::FrameBuffer> sceneFrameBuffer;
            RefPtr<Gfx::Image> gameDepthImage;
            // we are using stencils, but we can't assume gameDepthImage has stencil buffer, so we create our own's
            UniPtr<Gfx::Image> ourGameDepthImage;
            RefPtr<Shader> outlineByStencil, outlineByStencilDrawOutline;
            void DrawMainMenu();
            void RenderEditor(RefPtr<CommandBuffer> cmdBuf);
            void RenderSceneGUI(RefPtr<CommandBuffer> cmdBuf);
            void InitRendering();

            struct ImGuiData
            {
                UniPtr<Gfx::Buffer> vertexBuffer; // use single buffer to capture all the vertices
                UniPtr<Gfx::Buffer> indexBuffer;
                UniPtr<Gfx::Image> fontTex;
                UniPtr<Gfx::Image> editorRT;
                UniPtr<Gfx::ShaderResource> generalShaderRes;
                RefPtr<Gfx::ShaderProgram> shaderProgram;
                Gfx::ShaderConfig shaderConfig;

                RefPtr<Gfx::ShaderResource> GetImageResource();
                void ResetImageResource() { currCacheIndex = -1; }

                private:
                int currCacheIndex = -1;
                std::vector<UniPtr<Gfx::ShaderResource>> imageResourcesCache;
            };

            ImGuiData imGuiData;
    };
}
