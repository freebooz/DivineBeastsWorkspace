# LoadingIntegration（加载集成）

DivineBeastsApplicationFlow 不自己轮询加载对象，复用 UGamePlatformLoadingClientSubsystem（平台加载客户端子系统）。

每次 Assignment 创建独立 Loading Operation，并以 OperationId 关联。World、Character、Gameplay Data 和项目扩展通过 Notify*Ready 接口标记各自任务；失败/取消由 Loading 聚合成终态。

账号切换、流程重启或恢复时会取消旧 Loading Operation，避免旧任务推进新流程。
