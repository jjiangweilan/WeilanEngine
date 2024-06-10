#pragma once
#include <stdexcept>
namespace RenderGraph::Errrors
{
class Error : public std::logic_error
{
    using std::logic_error::logic_error;
};

class GraphCompile : public Error
{
    using Error::Error;
};

class NodeCreationError : public Error
{
    using Error::Error;
};
} // namespace RenderGraph::Errrors
