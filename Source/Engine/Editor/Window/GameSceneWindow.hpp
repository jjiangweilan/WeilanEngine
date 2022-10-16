#pragma once

#include "GfxDriver/Image.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Core/Graphics/Shader.hpp"
#include "../EditorContext.hpp"
namespace Engine::Editor {
    class GameSceneHandle
    {
        public:
            virtual void DrawHandle(RefPtr<CommandBuffer> cmdBuf) = 0;
            virtual void Interact(RefPtr<GameObject> go) = 0;
            virtual std::string GetNameID() = 0;
    };

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
            UniPtr<GameSceneHandle> activeHandle;
            UniPtr<Gfx::FrameBuffer> sceneFrameBuffer;

            RefPtr<Shader> simpleBlendShader;
            UniPtr<Gfx::Image> editorOverlayoutColor;
            UniPtr<Gfx::Image> outlineOffscreenColor;
            UniPtr<Gfx::Image> outlineOffscreenDepth;
            UniPtr<Gfx::RenderPass> outlinePass;
            UniPtr<Gfx::FrameBuffer> outlineFrameBuffer;
            UniPtr<Gfx::ShaderResource> outlineResource;

            RefPtr<Shader> outlineRawColor, outlineFullScreen;

            void UpdateRenderingResources(RefPtr<Gfx::Image> sceneColor, RefPtr<Gfx::Image> sceneDepth);
    };
} // namespace Engine::Editor
