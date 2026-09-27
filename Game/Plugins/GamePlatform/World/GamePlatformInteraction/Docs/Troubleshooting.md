# Troubleshooting（故障排查）

Focus 为空：检查 PlayerController ViewPoint、目标 Visibility 碰撞、InteractableComponent、Option Enabled/MaxDistance；Focus 只做本地候选，不代表服务器会接受。

Begin 返回 InteractorNotActive：检查 Owner/Controller/PlayerState 上是否存在 GameplayEligibility Provider，服务器是否已将 PlayerActive 设置为 true，以及 AvatarGeneration 是否有效。

Begin 返回 StaleTargetRevision/Generation：目标已经被另一玩家或服务器状态变化更新，应刷新 Focus/Snapshot 后重新请求，不能覆盖服务器 Revision。

Hold 被取消：检查 Gameplay资格、Target Generation/Revision、距离、LOS、Option状态和 Target EndPlay。当前没有 Server Rewind 或客户端时长完成捷径。
