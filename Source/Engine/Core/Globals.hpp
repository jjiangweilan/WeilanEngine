#pragma once

namespace Engine
{
    class Camera;
    class Globals
    {
        public:
            static Camera* GetMainCamera() {return mainCamera;};
            static void SetMainCamera(Camera* camera) {mainCamera = camera;};
        private:
            static Camera* mainCamera;
    };
}
