#pragma once

#include "Libs/Serialization/Serializable.hpp"
#include "TransformNode.hpp"

class TransformRoot : public Serializable
{
    DECLARE_SERIALIZATION();
    struct TransformPool
    {
    };

    std::vector<std::unique_ptr<TransformNode>> nodes;

public:
};
