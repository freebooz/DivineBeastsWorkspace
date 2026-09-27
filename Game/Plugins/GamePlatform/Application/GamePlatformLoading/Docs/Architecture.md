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

模块方向：`DivineBeastsArena（项目组合） → Loading（屏障） → Data（资源） → Core（中立类型）`。项目另行依赖Flow（流程），Flow和Loading互不链接。Session未接通，没有反向链接客户端模块来使Server碰巧通过。

每个实例独占Scope GUID、单个未释放操作、工厂、订阅与任务实例。操作和任务图在启动时复制冻结。Data跨实例共享资产的所有权仍在Data；Loading只有自己的实例期租约。没有GWorld或进程静态用户状态。

所有API、工厂、任务Start/Poll/Release都在游戏线程。任务不可重入修改服务；服务用调度保护拒绝此类操作。50毫秒核心Ticker用于有界批处理真实完成、世界状态和单调时间截止，不插值、不把时间当作成功条件。Data完成回调只写该尝试独占的结果对象；旧回调不持有新操作引用。

清理顺序：失效操作/取消 → 停止接纳结果 → 释放已创建任务自己的租约/监听 → 撤销世界弱声明。子系统退出先停Ticker并删除订阅，再取消和清理任务。成功任务保持资源直到显式Release；主工程节点成功退出会释放临时Loading租约，而组合根已有独立Probe/Flow租约仍由各自原有所有者持有。

当前限制：尚未经过UHT/UE编译；世界声明只涵盖基础可操作，不证明World Partition、导航、正式出生点或会话准入。快照是历史值，Ready后失去世界或资源必须用`IsReadyToPlay`重新判断。
