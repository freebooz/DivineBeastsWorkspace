# F23 交互焦点拥有关系（2026-09-30）

共享运行模块中的 Interactor 只采样本地拥有者焦点，服务器交互校验/提交路径继续持有权威。游戏线程 BeginPlay 注册 Pawn.ReceiveControllerChangedDelegate 或 Controller.OnPossessedPawnChanged；事件覆盖延迟 Possess、OnRep_Controller、失去拥有者和换 Pawn。集中 ReconcileLocalFocusSampling 在 Game/PIE、非命令行、非拆卸、非专服且有本地受控 Pawn 时建立一个已有周期采样计时器，身份失效立即停计时器并清空/发布焦点。

EndPlay 先解绑自身拥有关系委托，再按既有顺序清理计时器、在途交互及本地焦点。不增加逐帧业务轮询。IsLocalFocusSamplingActive 仅供诊断/回归读取当前计时器事实，不构成服务器准入。

涉及 Public/Components/GamePlatformInteractorComponent.h 与对应 Private 实现；Private/Tests/InteractionOwnershipLifecycleTests.cpp 创建无初始Controller的真实Pawn，再Possess/UnPossess核对计时器。该用例依赖 UE Automation，未由原生CMake执行；联机 OnRep_Controller 时序仍需双客户端运行。

## 验证与中文审核边界

本次修改已补上述责任、端侧、游戏线程、所有权、失败和取消合同；新增C++测试放在模块Private/Tests，插件Tests只持原生编译入口。原生结果与UE Automation/Editor/Client/Server编译分别记录于Game/Saved/Reviews/task3-repair-report.md，不能将纯规则通过当引擎时序通过。存量公开类型仍有逐字段中文说明缺口，本页不宣称全插件或全工作空间已经整体合规。
