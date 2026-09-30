# F03 导航请求所有权（2026-09-30）

服务位于服务器模块，游戏线程调用；调用方提供的 RequestId 仅作关联标识，WorldGeneration 和内部 OperationGeneration 共同保护异步操作。FindPathAsync 在调用原生导航调度前拒绝仍在飞的同 RequestId，旧查询保留其委托、计时器和结果所有权。完成回调同时核对 RequestId、世界、引擎 QueryId 和内部操作代次；超时核对世界与操作代次，旧超时不能终结后来复用关联 ID 的新操作。

重复在飞 ID 同步返回已发布 Unsupported 错误与无效句柄，不回调新请求；没有增加或改名错误枚举。Cancel/世界退出继续使用已有引擎中止和终态保护。历史公开取消句柄仍为 RequestId+WorldGeneration，因此调用方不应在同一世界复用已结束 ID 并用旧句柄取消新请求；本轮内部代次解决旧完成/超时，未改变发布句柄序列化身份。

涉及 GamePlatformNavigationServer 的 Subsystems/GamePlatformNavigationWorldSubsystem.h/.cpp、Private/Queries/NavigationRequestPolicy.h 与 Private/Tests/NavigationRequestPolicyTests.cpp。纯策略回归验证重复 ID 和旧超时，Tests/CMakeLists.txt 编译 Private/Tests 的真实测试文件；尚需 UE 原生 FindPathAsync 时序和取消测试。

## 验证与中文审核边界

本次修改已补上述责任、端侧、游戏线程、所有权、失败和取消合同；新增C++测试放在模块Private/Tests，插件Tests只持原生编译入口。原生结果与UE Automation/Editor/Client/Server编译分别记录于Game/Saved/Reviews/task3-repair-report.md，不能将纯规则通过当引擎时序通过。存量公开类型仍有逐字段中文说明缺口，本页不宣称全插件或全工作空间已经整体合规。
