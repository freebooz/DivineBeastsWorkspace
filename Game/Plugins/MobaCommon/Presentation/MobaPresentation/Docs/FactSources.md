# FactSources（事实来源）

事实优先采用事件驱动来源：Arena RepNotify/Native Delegate（竞技复制通知/原生委托）、GamePlatformCombat的 FGamePlatformCombatEvent（战斗事件）、Ability/Status/Character公共通知。

禁止通过每Tick轮询GameState比分、PlayerState统计、Health或GameplayTags推断事实。

暴击必须由上游明确的 FMobaPresentationCriticalFact（暴击确认事实）提供；当前Combat Event没有可靠Critical字段，因此MobaPresentation不会自行猜测暴击。
