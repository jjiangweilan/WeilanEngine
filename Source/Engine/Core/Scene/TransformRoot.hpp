#pragma once

#include "Libs/Serialization/Serializable.hpp"
#include "TransformNode.hpp"

class TransformRoot : public Serializable
{
    DECLARE_SERIALIZATION();

    std::vector<TransformNode> roots;

public:
};
