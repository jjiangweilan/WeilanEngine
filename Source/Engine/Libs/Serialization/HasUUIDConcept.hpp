#pragma once
#include "Libs/UUID.hpp"

namespace Engine
{
template <class T>
concept HasUUID = requires(T a, UUID uuid) {
                      a.GetUUID();
                      a.SetUUID(uuid);
                  };
}
