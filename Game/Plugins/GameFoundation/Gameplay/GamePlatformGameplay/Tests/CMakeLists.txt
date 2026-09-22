cmake_minimum_required(VERSION 3.20)
project(GamePlatformGameplayNativeTests LANGUAGES CXX)

enable_testing()

add_executable(GameplayPolicyTests
    ../Source/GamePlatformGameplay/Private/Tests/GameplayPolicyTests.cpp
)

target_compile_features(GameplayPolicyTests PRIVATE cxx_std_20)
target_compile_definitions(GameplayPolicyTests PRIVATE GAMEPLAY_NATIVE_TESTS=1)

if(MSVC)
    target_compile_options(GameplayPolicyTests PRIVATE /W4 /WX /permissive- /utf-8)
else()
    target_compile_options(GameplayPolicyTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME GameplayPolicyTests COMMAND GameplayPolicyTests)
