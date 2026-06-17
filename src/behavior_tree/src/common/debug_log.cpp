#include "behavior_tree/common/debug_log.hpp"

DebugLog::DebugLog(const std::string &name, const BT::NodeConfig &config)
    : BT::SyncActionNode(name, config)
{
}

BT::NodeStatus DebugLog::tick()
{
    auto msg_res = getInput<std::string>("message");
    if (!msg_res)
    {
        RCLCPP_ERROR(rclcpp::get_logger("DebugLog"), "Missing input [message]");
        return BT::NodeStatus::FAILURE;
    }

    RCLCPP_INFO(rclcpp::get_logger("DebugLog"), "// %s", msg_res.value().c_str());
    return BT::NodeStatus::SUCCESS;
}
