#include <rclcpp/rclcpp.hpp>

#include <behaviortree_cpp/behavior_tree.h>
#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/blackboard.h>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <string>

#include "behavior_tree/common/debug_log.hpp"

using namespace std;

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);

    auto node = rclcpp::Node::make_shared("main_runner");

    BT::BehaviorTreeFactory factory;
    factory.registerNodeType<DebugLog>("DebugLog");

    auto blackboard = BT::Blackboard::create();

    string package_share_directory = ament_index_cpp::get_package_share_directory("behavior_tree");
    string xml_file_path = package_share_directory + "/tree/main.xml";
    auto tree = factory.createTreeFromFile(xml_file_path, blackboard);
    tree.tickWhileRunning();

    return 0;
}
