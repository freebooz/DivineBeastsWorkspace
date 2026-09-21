# CMake generated Testfile for 
# Source directory: E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Tests
# Build directory: E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Saved/NativeTests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test([=[OnlineLogicTests]=] "E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Saved/NativeTests/Debug/OnlineLogicTests.exe")
  set_tests_properties([=[OnlineLogicTests]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Tests/CMakeLists.txt;15;add_test;E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test([=[OnlineLogicTests]=] "E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Saved/NativeTests/Release/OnlineLogicTests.exe")
  set_tests_properties([=[OnlineLogicTests]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Tests/CMakeLists.txt;15;add_test;E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test([=[OnlineLogicTests]=] "E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Saved/NativeTests/MinSizeRel/OnlineLogicTests.exe")
  set_tests_properties([=[OnlineLogicTests]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Tests/CMakeLists.txt;15;add_test;E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test([=[OnlineLogicTests]=] "E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Saved/NativeTests/RelWithDebInfo/OnlineLogicTests.exe")
  set_tests_properties([=[OnlineLogicTests]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Tests/CMakeLists.txt;15;add_test;E:/poject/feebooz/DivineBeastsWorkspace/Game/Plugins/GameFoundation/Application/GamePlatformOnline/Tests/CMakeLists.txt;0;")
else()
  add_test([=[OnlineLogicTests]=] NOT_AVAILABLE)
endif()
