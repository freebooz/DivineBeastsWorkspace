# 外部原生策略回归入口；世界服务的引擎过滤仍需真实UE自动化验收。
cmake_minimum_required(VERSION 3.20)
project(GamePlatformAIWorldPolicyTests LANGUAGES CXX)
enable_testing()
add_executable(AIWorldPolicyTests ../Source/GamePlatformAIServer/Private/Tests/AIWorldPolicyTests.cpp)
target_compile_features(AIWorldPolicyTests PRIVATE cxx_std_17)
target_compile_definitions(AIWorldPolicyTests PRIVATE GAMEPLATFORM_AI_NATIVE_TEST=1)
if(MSVC)
  target_compile_options(AIWorldPolicyTests PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME AI.WorldAuthority COMMAND AIWorldPolicyTests)

# 服务器AI激活资格真实值规则；引擎Tag链测试另由UE运行。
add_executable(AIAbilityActivationPolicyTests ../Source/GamePlatformAIServer/Private/Tests/AIAbilityActivationPolicyTests.cpp)
target_compile_features(AIAbilityActivationPolicyTests PRIVATE cxx_std_17)
target_compile_definitions(AIAbilityActivationPolicyTests PRIVATE GAMEPLATFORM_AI_NATIVE_TEST=1)
if(MSVC)
  target_compile_options(AIAbilityActivationPolicyTests PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME AI.AbilityActivation COMMAND AIAbilityActivationPolicyTests)

# 候选容量算法真实消费者回归，比较次数不代替Insights测量。
add_executable(AICandidateRetentionTests ../Source/GamePlatformAIServer/Private/Tests/AICandidateRetentionTests.cpp)
target_compile_features(AICandidateRetentionTests PRIVATE cxx_std_17)
target_compile_definitions(AICandidateRetentionTests PRIVATE GAMEPLATFORM_AI_NATIVE_TEST=1)
if(MSVC)
  target_compile_options(AICandidateRetentionTests PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME AI.CandidateRetention COMMAND AICandidateRetentionTests)
