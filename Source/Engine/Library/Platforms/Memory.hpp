#pragma once

#if _WINDOWS
#include "Library/Platforms/Windows/WindowsMemory.hpp"
#elif __APPLE__
#include "Library/Platforms/MacOS/MacOSMemory.hpp"
#endif
