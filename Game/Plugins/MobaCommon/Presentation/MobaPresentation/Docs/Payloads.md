# Payloads（载荷）

Runtime提供结构化载荷：FMobaPresentationHitPayload（命中）、CriticalPayload（暴击）、HealPayload（治疗）、ShieldPayload（护盾）、ControlPayload（控制）、AbilityCastPayload（技能施法）、ProjectilePayload（投射物表现）、AreaWarningPayload（范围预警）、StatusPayload（状态）、DeathPayload（死亡）、RespawnPayload（复活）。

载荷不携带ASC指针、UObject资源、Niagara、Sound、Widget或Backend DTO（后端数据对象）。Projectile Payload只描述表现事实，不创建Gameplay Projectile Actor（玩法投射物Actor）。
