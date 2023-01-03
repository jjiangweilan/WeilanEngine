#pragma once
#include <glm/glm.hpp>

namespace Engine::Rendering
{
    // Global shader resource that matches the layout of the first descriptor set in the shader
    class GlobalShaderResource
    {
        public:
            struct ShaderInternalLayout
            {
                glm::mat4 view;
                glm::mat4 projection;
                glm::mat4 viewProjection;
                glm::vec3 viewPos;
            };

        private:
    };
}
