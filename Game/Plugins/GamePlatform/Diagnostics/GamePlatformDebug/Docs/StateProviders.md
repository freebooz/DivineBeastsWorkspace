# StateProviders（状态提供者）

内建 Provider（状态提供者）：
Status（状态）、World（世界）、Character（角色）、Ability（技能/GAS）、Combat（战斗）、AI（人工智能）、Navigation（导航）、Online（在线服务）、Session（会话）、Loading（加载）、Telemetry（遥测）、Network（网络）。

真实读取原则：
- Combat（战斗）仅读取 UGamePlatformCombatComponent（平台战斗组件）的公开生命、护盾、死亡与 AvatarGeneration（化身代次）。
- AI（人工智能）仅读取 FGamePlatformAIStateSnapshot（AI公开复制状态快照）。
- Telemetry（遥测）仅读取 UGamePlatformTelemetrySubsystem（平台遥测子系统）的 GetDiagnostics（获取诊断）。
- GAS（Gameplay Ability System，玩法技能系统）使用 UAbilitySystemComponent（技能系统组件）的公开只读查询。

未公开稳定接口的数据返回 N/A（不可用）并说明原因，禁止猜值。

Provider 关闭时不采集；Expensive（高成本）Provider 只有 Manual/低频（手动/低频）上下文允许。V1 当前内建 Provider 最高为 Moderate（中成本）。