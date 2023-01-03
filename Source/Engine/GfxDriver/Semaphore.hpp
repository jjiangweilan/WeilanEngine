#pragma once
namespace Engine::Gfx
{
    class Semaphore
    {
        public:
            struct CreateInfo
            {
                bool signaled;
            };

            virtual ~Semaphore()
            {};
    };
}
