#pragma once

#include <rclcpp/rclcpp.hpp>
#include <behaviortree_cpp/action_node.h>


class DebugLog : public BT::SyncActionNode
{
public:
    DebugLog(const std::string &name, const BT::NodeConfig &config);

    BT::NodeStatus tick() override;

    static BT::PortsList providedPorts()
    {
        return {
            BT::InputPort<std::string>("message")
        };
    }
};
