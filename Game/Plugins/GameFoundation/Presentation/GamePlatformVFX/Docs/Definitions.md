# Definitions（数据定义）

根类：`UGamePlatformVFXDefinition`。

行为类型：Instant、Attached、Projectile、Beam、Area、Shield、Portal、Trail、World、Composite。

注意：10 种 Behavior（行为）不是 10 种 Actor（场景对象）。普通效果直接使用 `UNiagaraComponent`；只有真正需要独立空间实体时才使用 `AGamePlatformVFXHostActor`。

`StableId` 是逻辑身份，必须显式填写。文件名不能替代稳定 ID。
