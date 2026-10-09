# ShieldAndMitigation（护盾与减伤边界）

现行无Shield/MaxShield（盾永久GAS属性）。盾由`UGamePlatformShieldGameplayEffect（限时GE盾效果）`赋予Combat.State.Shielded（盾状态标签），服务器CombatComponent记录有界的临时独立吸收容量（不是可复制GAS属性）。普通伤害先加Buff、减目标减伤，再消耗仍有效GE盾容量，溢出扣Health（生命）；永久生命不低于0。

`Combat.Damage.BypassShield`允许服务器受信任的 CombatSpec 绕过护盾。没有公开客户端 RPC 可以上传该标签要求服务器接受。

纯函数 `FGamePlatformCombatMath::ResolveDamage`被运行组件和 Automation 测试共同使用，覆盖护盾吸收、溢出、绕盾和过量伤害。

最新DamageBonus（有符号增伤）与DamageReduction（有符号减伤）已由`UGamePlatformDamageExecutionCalculation（平台伤害执行）`读取同一个CombatAttributeSet的GAS聚合值，并调用通用纯加减公式；不保留护甲/法抗/穿透/暴击或百分比减伤分支。真实运行、双客户端同步和GE盾耗尽/失效仍须引擎验收。
