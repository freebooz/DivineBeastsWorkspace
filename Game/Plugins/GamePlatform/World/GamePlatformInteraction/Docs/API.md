# API（公开接口）

核心公开对象：`UGamePlatformInteractorComponent（交互发起组件）`、`UGamePlatformInteractableComponent（可交互组件）`、`IGamePlatformInteractable（可交互行为接口）`、`IGamePlatformInteractionEligibilityProvider（额外交互资格接口）`。

数据契约：`FGamePlatformInteractionOption`、`Request`、`Session`、`Result`、`Event`、`FocusSnapshot`和错误/取消枚举。Request 只包含 RequestId、TargetActor、TargetInstanceId、TargetGeneration、ObservedTargetRevision 和 OptionId。

客户端入口：`RefreshLocalFocus`、`BeginFocusedInteraction`、`CancelCurrentInteraction`和 `GetHoldProgress`。服务器 RPC 只有 Begin/Cancel 两个低频 Reliable（可靠）请求。

目标 Authority API 包含 SetOptions、SetInteractionEnabled、SetRemainingCharges、AdvanceTargetGeneration；会话占用和 Commit 由同插件内部控制。
