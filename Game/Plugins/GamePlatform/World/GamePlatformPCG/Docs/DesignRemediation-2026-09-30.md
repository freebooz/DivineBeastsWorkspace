# F15-PCG 活动请求与终态历史（2026-09-30）

双端运行模块只在 Game/PIE 的客户端或本地单机生成可选装饰；服务器碰撞、导航和出生结果继续遵循既有编辑器生成/烘焙边界。游戏线程世界门面拥有原生组件、委托、临时 Actor 与 Data 租约；生成中/清理中仍占活动槽，只有确认原生输出归零并释放租约后才进入 Cleaned。

清理终态完成派发后删除活动请求及该操作订阅，只保留最近128条自包含值快照，历史不保存 UObject、组件、租约或回调。当前活动请求上限4；历史容量不会停止下一次申请，第129次顺序请求可以接纳。终态历史仍在窗口内时 GetGenerationSnapshot 可读，Cancel/Release 幂等返回成功；淘汰后返回 StaleRequest，不能假定旧值句柄永远有效。跨世界仍核对完整句柄。回调返回后重新核对作用域，避免关闭后的归档访问。

涉及 Private/Subsystems/GamePlatformPCGWorldSubsystem.cpp、Private/Validation/PCGPolicy.h 及 Private/Tests/PCGPolicyTests.cpp。原生回归覆盖129次顺序申请的容量规则；引擎组件生成、输出清理与世界关闭仍待 UE 测试。异常退出不能证明原生清理时，既有实例租约留到 GI 退出，不宣称 Cleaned。

## 验证与中文审核边界

本次修改已补上述责任、端侧、游戏线程、所有权、失败和取消合同；新增C++测试放在模块Private/Tests，插件Tests只持原生编译入口。原生结果与UE Automation/Editor/Client/Server编译分别记录于Game/Saved/Reviews/task3-repair-report.md，不能将纯规则通过当引擎时序通过。存量公开类型仍有逐字段中文说明缺口，本页不宣称全插件或全工作空间已经整体合规。
