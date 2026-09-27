# TestingAndEvidence（测试与证据）

共享模块当前有2个 C++ Automation Test源码：AgentProfile结构/Invoker半径验证，以及 RequestHandle/PathLength/PathCost 契约验证。测试源码存在不等于 UE Automation 已实际执行。

`Build/Validation/VerifyNavigation.ps1`已实际执行静态生产门禁并通过：双模块、依赖方向、真实 NavigationSystem API token、异步 Cancel、Area/Filter/Modifier/SmartLink/Invoker、AI迁移、0个二进制资产。

全工作区三层结构门禁在导航迁移后执行通过：44插件、76模块、27条项目内依赖边、771项检查、无失败。最终收口后会再次核对。

运行型脚本均已实际调用但返回 not_executed：AI导航、Dynamic、WorldPartition、Scalability 因 `UE_ROOT/UnrealEditor-Cmd.exe unavailable`；Build/Cook 因 `UE_ROOT/RunUAT.bat unavailable`。
