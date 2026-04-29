#pragma once

#if defined(_WIN32)
#    if defined(WEILAN_ENGINE_EXPORTS)
#        define WEILAN_ENGINE_API __declspec(dllexport)
#    else
#        define WEILAN_ENGINE_API __declspec(dllimport)
#    endif
#else
#    define WEILAN_ENGINE_API
#endif
