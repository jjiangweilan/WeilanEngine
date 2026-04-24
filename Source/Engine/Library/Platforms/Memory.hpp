#pragma once

#if _WINDOWS
#include "Engine/Library/Platforms/Windows/WindowsMemory.hpp"
#elif __APPLE__
#include "Engine/Library/Platforms/MacOS/MacOSMemory.hpp"
#endif
