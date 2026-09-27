# WorldLifecycle（世界与流送生命周期）

Interactable EndPlay 会复制前先清理服务器活动 Session，占用归零并通知 Interactor 以 TargetDestroyed 取消；Interactor EndPlay 会清理 Focus/Hold Timer，并按 LevelTransition 或失效原因取消当前 Session。

`AdvanceTargetGeneration`复制新的 Generation、清空占用和 Commit 历史，并主动取消旧 Session；迟到 Session 因 Generation/Revision/ActiveSessions 检查不能 Commit 新目标代次。

没有全局静态 Target Registry，因此不同 World/PIE 不共享 Target 表。World Partition/Streaming 导致 Actor EndPlay 时使用同一清理路径。

当前没有真实 World Partition/Server Travel 运行测试证据；生命周期实现需在 UE 测试地图中继续验证。
