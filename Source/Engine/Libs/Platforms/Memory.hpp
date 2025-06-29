#pragma once

#if _WINDOWS
#include "Libs/Platforms/Windows/WindowsMemory.hpp"
#elif __APPLE__
#include "Libs/Platforms/MacOS/MacOSMemory.hpp"
#endif
