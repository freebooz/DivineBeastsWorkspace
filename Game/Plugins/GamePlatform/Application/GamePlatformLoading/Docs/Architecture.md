# 架构与生命周期

```text
GamePlatformLoading/                     # 唯一源码插件
├── GamePlatformLoading.uplugin           # 单Runtime模块与真实Data/Core依赖
├── Source/GamePlatformLoading/           # 编译模块
│   ├── GamePlatformLoading.Build.cs      # 双端构建依赖
│   ├── Public/                          # 稳定C++契约，不公开实例实现
│   │   ├── Interfaces/                  # IGamePlatformLoadingService与类型化任务接口
│   │   └── Types/                       # 操作、任务规格、句柄、诊断快照
│   └── Private/                         # 非公开实现
│       ├── GamePlatformLoadingModule.cpp # 仅模块登记，无启动业务
│       ├── Operations/LoadingPolicy.h   # 真实生产DAG、进度和屏障算法
│       ├── Subsystems/                  # 私有UGameInstanceSubsystem与工厂登记
│       ├── Tasks/LoadingBuiltinTasks.h  # Data租约和基础世界适配
│       └── Tests/                       # 原生生产算法及UE服务测试源码
├── Tests/CMakeLists.txt                  # 原生测试编译入口
├── README.md                            # 人工入口
└── Docs/                                # 十一份专题材料
```

模块方向：`DivineBeasts（项目组合） → Loading（屏障） → Data（资源） → Core（中立类型）`。项目另行依赖 ApplicationFlow（应用流程），Flow 和 Loading 保持解耦；Flow 负责“何时进入加载阶段”，Loading 只负责“该阶段的真实任务和 Ready 屏障”。Session（会话）当前仍没有可消费的正式公开准入服务，因此 Loading 不反向依赖客户端 Session 模块，也不通过私有状态或假任务让 Server 构建“碰巧通过”。

每个 GameInstance（游戏实例）独占 Scope GUID、单个未释放操作、工厂、订阅与任务实例。操作和任务图在启动时复制冻结。Data 跨实例共享资产的所有权仍在 GamePlatformData；Loading 只持有本操作申请的租约。没有 GWorld、Player0 假设或进程静态用户状态。Owner（所有者）可以是 GameInstance 自身、Outer 链属于该实例的对象，或 World→GameInstance 明确属于该实例的世界对象。

所有 API、工厂、任务 Start/Poll/Release 都在游戏线程。任务不可重入修改服务；服务用调度保护拒绝此类操作。调度采用按需 Ticker：空闲实例完全不注册 Ticker；Running（运行）状态以 50ms / 20Hz 有界采样真实任务和单调时间截止；Ready 后仍持有资源时仅以 500ms / 2Hz 检查弱 Owner 是否销毁；完全释放资源后停止 Ticker。进度不做每帧插值、不把时间当作成功条件。Data 完成回调只写本尝试独占结果；旧回调不能推进新操作或回退代次。

清理顺序：失效操作/取消 → 停止接纳结果 → 释放已创建任务自己的租约/监听 → 撤销世界弱声明。子系统退出先停Ticker并删除订阅，再取消和清理任务。成功任务保持资源直到显式Release；主工程节点成功退出会释放临时Loading租约，而组合根已有独立Probe/Flow租约仍由各自原有所有者持有。

性能边界：单操作最多256个任务、单任务最多64个直接依赖；启动校验使用哈希表/集合并建立稳定 TaskId→数组索引，运行期依赖查询不再反复线性扫描。状态订阅和自定义任务工厂各最多128项。这些是安全上限，不是最终性能预算；`FGamePlatformLoadingDiagnostics（加载诊断）` 提供 Ticker 次数、任务 Poll 次数、快照/订阅回调数量以及最近/最大 Tick 耗时，供 Debug/Telemetry 上层按需采样。

当前已通过 UE5.8 Editor／Client／Server 的 `GamePlatformLoading` 模块编译和 UHT；但世界声明仍只代表基础可操作，不证明 World Partition（世界分区）、导航、正式出生点、角色 GAS、UI/HUD 或会话准入。Snapshot（快照）是历史值，Ready 后失去世界、Owner 或资源时必须重新调用 `IsReadyToPlay`。当前没有内置 SessionReady：在线准入必须等 GamePlatformSession 提供真实公开服务后由客户端组合层注册适配任务；未接通前继续明确失败，不能模拟成功。
