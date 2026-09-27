# ConfigurationAndRun（配置与运行）

`Config/DefaultGamePlatformCombat.ini`定义 MaxDamageMagnitude、MaxHealingMagnitude、MaxHitDistance、MaxTraceOriginOffset、MaxSweepRadius、MaxControlDuration，以及 Self Damage/Healing 策略。

这些值是通用安全上限/默认策略，不是神兽联盟最终平衡数值。项目层最终公式应通过自己的 GameplayEffect Execution 扩展，而不是改写平台接口。

运行时 Actor 需要 `UGamePlatformAbilitySystemComponent`和 `UGamePlatformCombatComponent`；服务器 BeginPlay 会在 ASC 中补充 `UGamePlatformCombatAttributeSet`。

当前没有可用的锁定 UE5.8 Build，因此 Editor/Client/Server 实际启动说明只能停留在源码配置层，不能提供运行日志。
