# GameplayCharacterIntegration（Gameplay与角色集成）

`GamePlatformGameplay`原本没有 ServerPlayerActive 能力，本轮仅增加最小 `IGamePlatformGameplayEligibilityProvider`与 `UGamePlatformGameplayEligibilityComponent`，提供 ServerPlayerActive 和 AvatarGeneration 事实。Interaction 只读取，不拥有登录/准入/出生/死亡/重生。

Interactor 会在 Owner、Controller、PlayerState 及其 ActorComponent 中寻找 Gameplay Provider。额外 Dead/Stunned/Cinematic 等约束通过 `IGamePlatformInteractionEligibilityProvider`注入，因此 Interaction 不直接依赖 Combat/Character。

Development TestPawn 挂载 GameplayEligibility + Interactor，并在服务器 BeginPlay 标记 Active，用于后续网络测试胶水。正式 Character 插件仍是骨架，真实角色重生集成尚未执行。

AvatarGeneration 改变后 Active Hold Session 会在下一次服务器验证中因代次不匹配而取消；正式 Character 替换时应由 Gameplay/组合根推进 Generation。
