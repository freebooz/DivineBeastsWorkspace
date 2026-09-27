# PrerequisitesAndUnlocks（前置与解锁）

第一版 Prerequisite 支持 `QuestCompleted(OtherQuestId)`语义：接取时检查该玩家 Runtime 中对应 Quest Snapshot 是否为 Completed。

Definition 注册后执行依赖图循环检测；A→B→A 会使后注册的 Definition 校验失败，而不是运行时无限递归。自身依赖在单 Definition 验证阶段直接拒绝。

当前没有 Entitlement/Progression/Level 等前置条件，也不在 Quest 内偷偷访问这些未来系统。后续条件应扩展为稳定中立契约。

Completed 历史恢复优先于当前 Definition 版本，因此已完成前置不会因内容版本升级自动回退成未解锁。
