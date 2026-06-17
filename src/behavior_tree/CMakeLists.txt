cmake_minimum_required(VERSION 3.8)
project(behavior_tree)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# package.xml で依存関係は宣言しているので、 ament_cmake_auto の ament_auto_find_build_dependencies() を使用して package の依存解決を行う
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()

ament_auto_add_library(bt_nodes SHARED
  src/common/debug_log.cpp
)

ament_auto_add_executable(main_runner src/runner/main_runner.cpp)
target_link_libraries(main_runner bt_nodes)

# xml ファイルを参照するため、 install/ に tree ディレクトリの中身をコピーする
install(DIRECTORY tree DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_auto_package()
