#pragma once

#include "GfxDriver/Image.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Core/Graphics/Shader.hpp"
#include "../EditorContext.hpp"
namespace Engine::Editor {
    class GameSceneWindow {
        public:
            GameSceneWindow(RefPtr<EditorContext> editorContext);

            // image: the image to display at this window
            void Tick(RefPtr<Gfx::Image> sceneColor, RefPtr<Gfx::Image> sceneDepth);
            void RenderSceneGUI(RefPtr<CommandBuffer> cmdBuf);

        private:
            RefPtr<EditorContext> editorContext;
            RefPtr<Gfx::Image> sceneColor;
            RefPtr<Gfx::Image> sceneDepth;
            UniPtr<Gfx::Image> newSceneColor;
            UniPtr<Gfx::Image> newSceneDepth;
            UniPtr<Gfx::RenderPass> scenePass;
            UniPtr<Gfx::FrameBuffer> sceneFrameBuffer;
            RefPtr<Shader> outlineByStencil, outlineByStencilDrawOutline;

            void UpdateRenderingResources(RefPtr<Gfx::Image> sceneColor, RefPtr<Gfx::Image> sceneDepth);
    };
} // namespace Engine::Editor
