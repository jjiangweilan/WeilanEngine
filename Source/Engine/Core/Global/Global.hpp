#include "ProjectConfig.hpp"
#include "Code/Ptr.hpp"

namespace Engine
{
    class Global
    {
        public:
            static RefPtr<Global> Instance() { return instance; };
            static void InitSingleton() { if (instance == nullptr) instance = MakeUnique<Global>(); };

            ProjectConfig projectConfig;
        private:

            static UniPtr<Global> instance;
    };
}
