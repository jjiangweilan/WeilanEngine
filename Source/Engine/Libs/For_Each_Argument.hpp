#pragma once
#include <utility>

template <typename TF, typename... Ts>
void for_each_argument(TF&& f, Ts&&... xs)
{
    (void)(int[]){(f(std::forward<Ts>(xs)), 0)...};
}
