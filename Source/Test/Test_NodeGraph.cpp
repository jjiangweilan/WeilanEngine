#pragma once
#include "NodeGraph/NodeGraph.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

// class MyNode : public Node
//{
// public:
//     MyNode(const std::string& name) : name(name) {}
//
//     std::string name;
// };
//
// class OutputPort : public NodePort
//{
// public:
//     NGRAPH_DEFAULT_PORT_CTOR(OutputPort, Output)
// };
//
// class InputPort : public NodePort
//{
// public:
//     NGRAPH_DEFAULT_PORT_CTOR(InputPort, Input)
//
//     bool IsCompatible(PortDataType id) override
//     {
//         if (id == typeid(OutputPort)) return true;
//         return false;
//     }
// };

TEST(NodeGraph, Test0)
{
    /* NodeGraph graph;
     auto node0 = graph.AddNode<MyNode>("0");
     auto node1 = graph.AddNode<MyNode>("1");
     auto node2 = graph.AddNode<MyNode>("2");
     auto node3 = graph.AddNode<MyNode>("3");
     auto node4 = graph.AddNode<MyNode>("4");
     auto node5 = graph.AddNode<MyNode>("5");
     auto node6 = graph.AddNode<MyNode>("6");
     auto node7 = graph.AddNode<MyNode>("7");

     auto node0_out0 = node0->AddPort<OutputPort>();
     auto node0_out1 = node0->AddPort<OutputPort>();

     auto node1_in0 = node1->AddPort<InputPort>();
     auto node1_out0 = node1->AddPort<OutputPort>();

     auto node2_in0 = node2->AddPort<InputPort>();
     auto node2_out0 = node2->AddPort<OutputPort>();

     auto node3_out0 = node3->AddPort<OutputPort>();
     auto node3_out1 = node3->AddPort<OutputPort>();

     auto node4_in0 = node4->AddPort<InputPort>();
     auto node4_out0 = node4->AddPort<OutputPort>();

     auto node5_in0 = node5->AddPort<InputPort>();
     auto node5_in1 = node5->AddPort<InputPort>();
     auto node5_in2 = node5->AddPort<InputPort>();
     auto node5_out0 = node5->AddPort<OutputPort>();

     auto node6_in0 = node6->AddPort<InputPort>();
     auto node6_in1 = node6->AddPort<InputPort>();

     auto node7_in0 = node7->AddPort<InputPort>();
     auto node7_in1 = node7->AddPort<InputPort>();

     node0_out0->Connect(node1_in0);
     node0_out1->Connect(node2_in0);
     node3_out0->Connect(node2_in0);
     node3_out1->Connect(node4_in0);
     node3_out1->Connect(node6_in1);
     node1_out0->Connect(node5_in0);
     node2_out0->Connect(node5_in1);
     node2_out0->Connect(node6_in0);
     node4_out0->Connect(node5_in2);
     node4_out0->Connect(node7_in1);
     node5_out0->Connect(node7_in0);

     const auto& list = graph.SortByDepth();
     for (auto& l : list)
     {
         std::cout << static_cast<MyNode*>(l.get())->name << std::endl;
     }*/
}
