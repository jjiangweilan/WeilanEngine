#pragma once
#include <utility>

#define UNUSED(x) (void)(x)

template <typename TF, typename... Ts>
void for_each_argument(TF&& f, Ts&&... xs)
{
    int x[] = {(f(std::forward<Ts>(xs)), 0)...};
    UNUSED(x);
}
