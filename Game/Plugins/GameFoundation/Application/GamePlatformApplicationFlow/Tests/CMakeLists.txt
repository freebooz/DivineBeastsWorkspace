cmake_minimum_required(VERSION 3.20)
project(GamePlatformApplicationFlowTests LANGUAGES CXX)

# 编译插件正在使用的生产核心。该入口不编译 UE 反射或子系统适配层。
set(FLOW_PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../Source/GamePlatformApplicationFlow/Private")
find_package(Threads REQUIRED)
add_executable(ApplicationFlowTests
    Standalone/ApplicationFlowTests.cpp
    "${FLOW_PRIVATE}/Execution/ApplicationFlowExecutor.cpp")
target_include_directories(ApplicationFlowTests PRIVATE "${FLOW_PRIVATE}")
target_compile_features(ApplicationFlowTests PRIVATE cxx_std_17)
target_link_libraries(ApplicationFlowTests PRIVATE Threads::Threads)
if(MSVC)
    target_compile_options(ApplicationFlowTests PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(ApplicationFlowTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
enable_testing()
add_test(NAME ApplicationFlow.Core COMMAND ApplicationFlowTests)
