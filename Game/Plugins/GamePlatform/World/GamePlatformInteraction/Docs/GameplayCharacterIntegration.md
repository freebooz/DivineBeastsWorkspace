# GameplayCharacterIntegration（Gameplay与角色集成）

`GamePlatformGameplay`原本没有 ServerPlayerActive 能力，本轮仅增加最小 `IGamePlatformGameplayEligibilityProvider`与 `UGamePlatformGameplayEligibilityComponent`，提供 ServerPlayerActive 和 AvatarGeneration 事实。Interaction 只读取，不拥有登录/准入/出生/死亡/重生。

Interactor（交互发起器）会按 Owner（拥有者）→受控 Pawn（角色）→PlayerController（玩家控制器）→PlayerState（玩家状态）及其 ActorComponent（实体组件）寻找 Gameplay Provider（玩法资格提供者）；即使 Interactor 挂在 PlayerController，也能发现 Pawn 上的资格组件。额外 Dead/Stunned/Cinematic（死亡/眩晕/过场）等约束通过 `IGamePlatformInteractionEligibilityProvider（额外交互资格接口）`注入，因此 Interaction 不直接依赖 Combat/Character（战斗/角色）插件。

Development TestPawn 挂载 GameplayEligibility + Interactor，并在服务器 BeginPlay 标记 Active，用于后续网络测试胶水。正式 Character 插件仍是骨架，真实角色重生集成尚未执行。

AvatarGeneration 改变后 Active Hold Session 会在下一次服务器验证中因代次不匹配而取消；正式 Character 替换时应由 Gameplay/组合根推进 Generation。
