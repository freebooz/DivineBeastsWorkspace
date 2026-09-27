# EventIngestion（事件接入）

`FGamePlatformQuestEvent`包含 EventId、EventType、PlayerId、PlayerRuntimeId、CharacterId、WorldId、RegionId、SourceEntityId、TargetEntityId、SemanticTags、NumericValue、OccurredAtUtc 和 SourceGeneration。EventId 必须是稳定事实编号，不能仅用时间戳。

`DBAServer/QuestIntegration/UDBAQuestEventAdapterComponent（项目任务事件适配组件）`监听玩家服务器 CombatEvent 和 InteractionEvent：Combat Death 且 SourceActor 为当前玩家时映射 `Combat.Defeat`；Interaction 只有 Committed/PickupConsumed/HarvestCompleted 映射 `Interaction.Committed`。客户端 Focus 不会推进任务。

Region 目前没有现成服务器 RegionEntered 事件总线，因此适配组件只提供 Authority-only（仅服务器） `SubmitRegionEntered`入口；在服务器世界系统未形成可信事实前，不允许把客户端区域提示当 Objective 完成。

Gameplay/Tutorial 其它可信事实可以通过 `SubmitTrustedGameplayEvent`注入，但调用者必须位于服务器组合层。基础 Combat/Interaction/AI/Navigation 插件均不反向依赖 Quest。
