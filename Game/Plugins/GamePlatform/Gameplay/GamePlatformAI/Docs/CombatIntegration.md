# CombatIntegration（战斗集成）

AI 不直接修改 Health，也不建立第二套 Damage RPC。攻击出口为 `UGamePlatformAbilitySystemComponent::TryActivateAbilitiesByTag`：AIDefinition只配置服务器受信任 PrimaryAbilityTag、AttackRange 和退避时间；AbilitySystem负责技能是否可激活，Combat负责最终伤害/死亡。

当前 AbilitySystem 仍是最小前置实现，没有完整 AbilityId/AbilitySet 授予体系，因此真实 Test Ability、Cooldown/Cost和 Combat结算联调必须等正式 GAS 开发资产后验证；源码只建立合法调用边界。

CombatEvent：AI死亡时 StopBrain/StopMovement/清 Target/候选并公开 Dead；AI受伤时 Source加入候选；RespawnReset 清旧感知并恢复 Brain。

Stun：停止Move并 PauseLogic，解除后 ResumeLogic；Silence：AI仍可移动但攻击条件失败，并避免高频无意义激活。Root：当前 GamePlatformCombat 未实现 Root，因此本轮不适用，未将 Root伪装成 Stun。
