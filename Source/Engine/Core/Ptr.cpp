#include "Ptr.hpp"
ObjectLifetimeManager* ObjectLifetimeManager::Singleton()
{
    static ObjectLifetimeManager instance;
    return &instance;
}
