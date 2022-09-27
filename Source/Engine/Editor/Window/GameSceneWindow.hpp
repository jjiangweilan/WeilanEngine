#pragma once

#include "GfxDriver/Image.hpp"
#include "../EditorContext.hpp"
namespace Engine::Editor {
    class GameSceneWindow {
        public:
            GameSceneWindow(RefPtr<EditorContext> editorContext) :
                            editorContext(editorContext) {};

            // image: the image to display at this window
            void Tick(RefPtr<Gfx::Image> image);

        private:
            RefPtr<EditorContext> editorContext;
    };
} // namespace Engine::Editor
