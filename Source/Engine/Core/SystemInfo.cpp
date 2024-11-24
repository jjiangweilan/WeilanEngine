#include "SystemInfo.hpp"

SystemInfo& SystemInfo::Singleton()
{
    static SystemInfo i;
    return i;
}
