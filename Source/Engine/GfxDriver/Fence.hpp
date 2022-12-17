#pragma once

namespace Engine::Gfx
{
    class Fence
    {
        public:
            struct CreateInfo
            {
                bool signaled = false;
            };
            virtual ~Fence(){};
    };
}
