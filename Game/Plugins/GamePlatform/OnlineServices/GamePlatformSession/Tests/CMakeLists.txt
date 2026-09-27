cmake_minimum_required(VERSION 3.20)
project(GamePlatformSessionStateTests LANGUAGES CXX)
enable_testing()
add_executable(SessionStateTests
    ../Source/GamePlatformSession/Private/State/SessionConnectionState.cpp
    ../Source/GamePlatformSession/Private/Tests/SessionStateTests.cpp)
target_compile_features(SessionStateTests PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(SessionStateTests PRIVATE /utf-8 /W4)
endif()
target_compile_definitions(SessionStateTests PRIVATE GAMEPLATFORM_SESSION_NATIVE_TEST=1)
add_test(NAME SessionState COMMAND SessionStateTests)
