# 测试与证据追溯

日期2026-09-21。N表示`Private/Tests/LoadingPolicyTests.cpp`直接测试生产`Operations/LoadingPolicy.h`；U表示`Private/Tests/LoadingServiceTests.cpp`真实实例子系统自动化源码。N的Debug/Release各为1个CTest集合、39条断言，不是39次引擎运行。原生测试不创建UObject。

实际命令为`VerifyLoading.ps1 -NativeTests -Targets Editor -EngineRoot <本机UE5.8> -CMake <本机cmake>`；证据根`Saved/Validation/GamePlatformLoading/eae0ffc0-d541-40a5-bd0a-8933e54ff0eb`，`Test-Debug/stdout.log`、`Test-Release/stdout.log`、每项`result.json`。UE失败证据与汇总见仓库验证文档。

| 需求 | 实现与用例 | 状态 | 限制/证据 |
| --- | --- | --- | --- |
| L01 单Required成功 | Operation::Complete，N Single | 通过 | N日志 |
| L02 多Required全部完成 | Evaluate，N Multiple | 通过 | 依赖完成前不Ready |
| L03 Required失败 | Complete，N Failed | 通过 | 整体Failed |
| L04 Optional失败可就绪且诊断保留 | N Optional | 通过 | Error不抹除 |
| L05 真回退才降级 | N Fallback/FallbackFailed | 通过 | 原尝试迟到被拒绝 |
| L06 依赖顺序 | Startable，N Multiple/Blocked | 通过 | 可选失败父不能满足必需子 |
| L07 未知依赖 | Start，N Invalid | 通过 | 启动拒绝 |
| L08 环依赖 | Start，N Cyclic | 通过 | 非递归校验 |
| L09 重复任务 | Start，N Invalid | 通过 | 启动拒绝 |
| L10 进度Clamp/单调 | Report，N Single | 通过 | -1、2及非有限输入 |
| L11 非法权重 | Start，N BadWeight | 通过 | 0/无穷拒绝 |
| L12 100%不等于Ready | N Single；U Lifecycle | 通过 | 仅N通过，U未执行 |
| L13 操作取消 | Cancel，N Cancelled | 通过 | Cancelled终态 |
| L14 取消后迟到 | N Cancelled | 通过 | Complete返回false |
| L15 超时完成竞争 | Expire，N Timeout/Cancelled | 通过 | 超时先接纳后成功被拒绝 |
| L16 A取消后B隔离 | N Cancelled新代次 | 通过 | 不替代真实异步UE验证 |
| L17 双PIE隔离 | Scope、U Lifecycle中的独立实例 | 未执行 | 原生两个Operation不是双PIE |
| L18 Data需求期保留 | FLoadingDataTask、U RealLeases | 未执行 | 缺UE编译及真实定义 |
| L19 Data按所有权释放 | ReleaseTasks、U RealLeases | 未执行 | 原生算法不证明UE租约 |
| L20 Session失败传播 | 尚无真实公开服务适配 | 未执行 | 前置阻塞，无模拟通过 |
| L21 Session目标不匹配 | 尚无真实公开服务适配 | 未执行 | 同上 |
| L22 World目标不匹配 | FLoadingWorldEvidence，U Lifecycle | 未执行 | UE自动化与真实地图未运行 |
| L23 Flow取消旧节点隔离 | DBALoadingFlowNode + SubmitEvent | 未执行 | 仅既有Flow纯算法回归通过 |
| L24 工厂撤销 | Register/Unregister，U Lifecycle | 未执行 | 有测试源码但未编译执行 |
| L25 Server无客户端UI依赖 | uplugin/Build.cs静态检查 | 通过 | 仅静态依赖；Server构建失败/Cook未执行 |

FoundationLoadingOnly（真实两定义+Sandbox）、FoundationSessionLoading、两个PIE及Client/Server干净Cook均未执行。没有截图、运行视频或人工签名。

前置回归本轮实际执行Core11测试、Data20场景、Flow31场景、Host原生测试，全部Debug退出0；路径`Saved/Validation/GamePlatformLoading/Regression-4a3985fe-9e95-49a4-8831-bfbdde05bbe0`，分别`Core-test.log`、`Data-test.log`、`Flow-test.log`、`Host-test.log`。这些不升级为UE集成通过。
