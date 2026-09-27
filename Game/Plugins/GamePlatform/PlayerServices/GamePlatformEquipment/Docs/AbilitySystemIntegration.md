# AbilitySystemIntegration（能力系统集成）

EquipmentServer 使用 `FGamePlatformEquipmentGASGrantPort（装备GAS授予端口）`和每槽位 `FGamePlatformEquipmentGameplayGrantHandle（玩法授予句柄）`。

授予使用真实 GAS GiveAbility（授予能力）与 ApplyGameplayEffectToSelf（应用玩法效果）；卸下只 ClearAbility/RemoveActiveGameplayEffect 自己记录的 Handle，不调用 ClearAllAbilities。

当前 GamePlatformAbilitySystem 仍明确处于 scaffold（工程骨架），没有正式 AbilitySet 目录/Grant API。因此 Equipment 只定义 Resolver Port；非空 AbilitySet/Effect ID 若没有真实 Resolver，会明确 Grant 失败，不伪造成功。