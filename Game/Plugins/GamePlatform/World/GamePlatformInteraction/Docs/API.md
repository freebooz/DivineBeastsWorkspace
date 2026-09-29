# API（公开接口）

核心公开对象：`UGamePlatformInteractorComponent（交互发起组件）`、`UGamePlatformInteractableComponent（可交互组件）`、`IGamePlatformInteractable（可交互行为接口）`、`IGamePlatformInteractionEligibilityProvider（额外交互资格接口）`。

数据契约：`FGamePlatformInteractionOption（交互选项）`、`Request（请求）`、`Session（会话）`、`Result（结果）`、`Event（事件）`、`FocusSnapshot（焦点快照）`和错误/取消枚举。Request 只包含 RequestId、TargetActor、TargetInstanceId、TargetGeneration、ObservedTargetRevision 和 OptionId；服务器以 TargetInstanceId 精确查找组件，不再假设一个 Actor 只能存在一个可交互组件。

客户端入口：`RefreshLocalFocus（刷新本地焦点）`、`BeginFocusedInteraction（开始当前焦点交互）`、`CancelCurrentInteraction（取消当前交互）`和 `GetHoldProgress（获取长按进度）`。服务器 RPC 只有 Begin/Cancel 两个低频 Reliable（可靠）请求；重复 Begin/Cancel 会优先命中幂等缓存或活动会话，再进入限流判断，避免网络重试改变原始结果。

目标 Authority API（服务器权威接口）包含 SetOptions（设置选项）、SetInteractionEnabled（设置可交互状态）、SetRemainingCharges（设置剩余采集次数）、AdvanceTargetGeneration（推进目标代次）；会话占用、失效弱引用清理和 Commit（提交）由同插件内部控制。Revision（修订号）用于 Begin 阶段防止客户端基于旧状态发起请求，活动会话则由 Generation、活动占用和显式取消共同保证一致性。

`Custom（自定义）`提交采用 Fail-Closed（失败关闭）默认策略：`IGamePlatformInteractable::CommitInteraction（可交互行为提交）`若未由具体目标显式实现，默认返回失败；禁止在没有业务副作用的情况下把交互误报为 Completed（已完成）。
