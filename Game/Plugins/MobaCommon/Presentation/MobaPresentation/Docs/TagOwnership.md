# TagOwnership（标签所有权）

MobaPresentationRuntime（MOBA共享表现语义模块）是所有 Moba.* 表现语义标签的唯一Owner Module（所有者模块）。

| Tag范围 | Owner（所有者） | Source Fact（事实来源） |
| --- | --- | --- |
| Moba.Combat.* | MobaPresentationRuntime | GamePlatformCombat公共事实 |
| Moba.Ability.* | MobaPresentationRuntime | Ability公共通知/适配事实 |
| Moba.Status.* | MobaPresentationRuntime | 复制Tag/Effect或可靠公共通知 |
| Moba.Character.* | MobaPresentationRuntime | Combat/Character生命周期事实 |
| Moba.Arena.* | MobaPresentationRuntime | GamePlatformArena复制事实 |

FMobaPresentationSemanticRegistry（语义注册表）验证重复所有权、无效标签、Moba前缀以及已取消旧系统标签。Client/Server真实Tag Dictionary一致性仍需UE5.8构建验证。
