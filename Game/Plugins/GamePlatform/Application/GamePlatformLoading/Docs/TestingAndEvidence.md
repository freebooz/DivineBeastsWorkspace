# 测试与证据追溯

更新日期2026-09-27。N表示 `Private/Tests/LoadingPolicyTests.cpp` 直接测试生产 `Operations/LoadingPolicy.h`；U表示 `Private/Tests/LoadingServiceTests.cpp` 真实实例子系统自动化源码。本轮 N 的 Debug/Release 各为1个 CTest 集合、46条断言，不是46次引擎运行；原生测试不创建 UObject。U 测试源码已进入 Editor/Client/Server 模块编译，但由于当前源码引擎没有可启动的 UnrealEditor/UnrealEditor-Cmd，本轮未实际运行 UE Automation。

本轮原生证据目录为 `Saved/Validation/GamePlatformLoading/NativeCurrent`，使用 CMake 3.31.6-msvc6、VS2022 BuildTools、MSVC 19.44.35228、Windows SDK 10.0.26100.0；Debug/Release 均退出0并输出 `46 assertions passed`。UE5.8 模块构建使用 `D:/UnrealEngine-5.8.0-release/Engine/Build/BatchFiles/Build.bat` 与引擎自带 .NET 10，Editor/Client/Server 的 `-Module=GamePlatformLoading` 均成功。2026-09-21 的历史失败证据仍保留在仓库生产验证文档中。

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
| L18 Data需求期保留 | FLoadingDataTask、U RealLeases | 部分验证 | UE模块编译通过；真实Definition资产仍为0，U RealLeases未运行 |
| L19 Data按所有权释放 | ReleaseTasks、U RealLeases | 部分验证 | 服务测试源码已编译；真实资产自动化未运行，不能用模块编译代替租约运行证据 |
| L20 Session失败传播 | 尚无真实公开服务适配 | 未执行 | 前置阻塞，无模拟通过 |
| L21 Session目标不匹配 | 尚无真实公开服务适配 | 未执行 | 同上 |
| L22 World目标不匹配 | FLoadingWorldEvidence，U Lifecycle | 未执行 | UE自动化与真实地图未运行 |
| L23 Flow取消旧节点隔离 | DBALoadingFlowNode + SubmitEvent | 未执行 | 仅既有Flow纯算法回归通过 |
| L24 工厂撤销 | Register/Unregister，U Lifecycle | 部分验证 | 测试源码已通过三端模块编译；UE Automation未运行 |
| L25 Server无客户端UI依赖 | uplugin/Build.cs + Server模块构建 | 通过（代码端） | `DivineBeastsArenaServer -Module=GamePlatformLoading`成功；Server Cook/Stage仍未执行 |
| L26 空闲零Ticker | SelectSamplingMode、U Diagnostics | 原生通过/UE未运行 | N证明Idle策略；U源码断言空闲实例不安排Ticker，待UE Automation实跑 |
| L27 Active/Retained调度 | SelectSamplingMode、ScheduleTicker | 原生通过/UE编译通过 | Active=20Hz、Retained=2Hz的实际时间行为仍需运行期采样验证 |
| L28 Loading诊断 | FGamePlatformLoadingDiagnostics、U Lifecycle | 编译通过 | 指标字段及调用链已编译；数值正确性与性能阈值待UE运行/压力测试 |

Editor/Client/Server 的 GamePlatformLoading 模块编译已通过。FoundationLoadingOnly（真实两定义+Sandbox）、FoundationSessionLoading、两个PIE、Client/Server干净Cook/Stage和人工签审仍未执行；当前工程真实 `.uasset/.umap` 为0，也没有运行截图或视频。

本轮针对 Loading 本身实际执行：Native Debug/Release 各1/1、46条策略断言全部通过；UE5.8 Editor/Client/Server 三个 `GamePlatformLoading` 模块构建全部通过。Core/Data 等上层/下层插件具有各自独立验证记录，本文件不把其他会话或历史回归重复计算为本轮 Loading 证据。
