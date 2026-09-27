# SmartLinks（智能导航链接）

`AGamePlatformNavigationSmartLinkProxy`继承 UE `ANavLinkProxy`，提供 NavigationLinkId、TraversalType、Enable/Disable、TraversalRequested 和 CompleteTraversal。UE Smart Link 可动态启停且通过 `ResumePathFollowing`把控制交还 PathFollowing。

当前 C++桥接不直接修改 Door/Character/Ability/Interaction 业务状态。真实资产必须在原生 `ReceiveSmartLinkReached（到达智能链接事件）`中调用平台 `NotifyTraversalRequested`，业务适配完成后再 `CompleteTraversal`。该原生事件是 BlueprintImplementableEvent，因此没有 `.uasset` 时不能伪称到达回调已运行。

CompleteTraversal 成功时 ResumePathFollowing；失败时禁用当前 SmartLink 并对 AIController StopMovement，要求上层重新规划/放弃，不 Teleport、不无限卡在 Link 上。

SmartLink 真实启用/禁用、Path 使用 Link、失败回退和 Actor 销毁清理均为未执行。
