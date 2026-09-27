# SecurityAndRobustness（安全与稳健性）

AI Brain只在服务器执行。客户端没有 RPC/接口可设置AI Target、Blackboard、Perception、PublicState或最终Combat结果。

Target必须实现/提供中立 Eligibility，EntityId/Generation必须匹配，Actor必须同World、未死且未超Leash；候选超时/销毁会被清理。排序不依赖容器偶然顺序。

攻击只调用 GAS `TryActivateAbilitiesByTag`，不 SetHealth、不直接 ApplyDamage；Stun/Death来自 Combat真实标签/事件。无Faction/FiveCamp/Element恢复。

异步资产Handle在UnPossess取消，回调校验 AI Generation。Server-only源代码不依赖UI/VFX/InputClient/GamePlatformNavigation，不每帧GetAllActorsOfClass。
