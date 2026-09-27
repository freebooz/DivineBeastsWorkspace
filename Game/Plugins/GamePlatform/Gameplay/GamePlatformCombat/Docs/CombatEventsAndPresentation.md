# CombatEventsAndPresentation（战斗事件与表现）

`FGamePlatformCombatEvent`用于 C++ 上层规则和诊断，包含 EventId、事件类型、Source/Target、AvatarGeneration、请求/应用数值、护盾/生命结算、ResultTags、ImpactPoint/Normal 和 WorldContextGeneration。普通 Delegate 本身不跨网络。

GameplayCue 使用 `GameplayCue.Combat.Damage/Healing/Control/Death`表达 GAS 标准表现事实，并携带 RawMagnitude、位置、法线、Instigator 和 EffectCauser。

Combat 不 Spawn Niagara、不 Play Sound、不 CreateWidget、不 CameraShake；VFX/SFX/Animation/UI 后续消费 GameplayCue/复制事实。

虽然 `GamePlatformPresentationCore`现已存在，本轮未给 Combat 添加该依赖，以保持 CombatEvent + GameplayCue 在表现插件缺失时也能独立工作。
