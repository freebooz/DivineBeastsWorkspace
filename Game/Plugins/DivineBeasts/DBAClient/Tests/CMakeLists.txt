cmake_minimum_required(VERSION 3.20)
project(DivineBeastsReadinessTests LANGUAGES CXX)
enable_testing()
# 只验证生产事实校验器；不模拟UE世界、不替代会话准入或引擎集成验收。
add_executable(DivineBeastsReadinessTests
    ../Source/DivineBeastsApplicationFlowClient/Private/Tests/DivineBeastsReadinessFactsTests.cpp)
target_compile_features(DivineBeastsReadinessTests PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(DivineBeastsReadinessTests PRIVATE /utf-8 /W4 /WX)
endif()
target_compile_definitions(DivineBeastsReadinessTests PRIVATE DBA_READINESS_NATIVE_TEST=1)
target_include_directories(DivineBeastsReadinessTests PRIVATE
    ../Source/DivineBeastsApplicationFlowClient/Private)
add_test(NAME DivineBeasts.ReadinessFacts COMMAND DivineBeastsReadinessTests)
