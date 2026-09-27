# AIModel（AI模型）

每个 AI Pawn 使用 `UGamePlatformAIStateComponent`保存 AIEntityId、AIDefinitionId、PublicState、CurrentTargetEntityId、Movement/Combat Intent Tag、StateRevision 和 AIInstanceGeneration；持续状态复制给客户端，内部候选/Blackboard/路径不复制。

服务器 WorldSubsystem 只在非客户端世界工作：一次性处理世界已存在 Actor，并监听后续 Actor Spawn；带 AIStateComponent 且尚无 Controller 的 Pawn 自动生成 ServerOnly `AGamePlatformAIController`并 Possess。

`UGamePlatformAITargetComponent`提供 TargetEntityId、TargetGeneration、Enabled 状态，项目/MOBA以后可通过 `IGamePlatformAITargetEligibilityProvider`扩展安全区、队伍或任务关系，而不是恢复 Faction/FiveCamp/Element。

AIInstanceGeneration 在 UnPossess/复用边界推进；异步 Definition/Brain 资产回调必须匹配当前 Generation 才继续。
