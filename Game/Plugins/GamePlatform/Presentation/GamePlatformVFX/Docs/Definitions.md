# Definitions（VFX定义）

Definition（定义资产）描述“如何执行一个视觉效果”，不保存英雄、生肖、MOBA胜负等上层业务语义。

当前 P0 已实现：Instant（瞬时）、Attached（附着）、Beam（光束）、Area（区域）、Composite（复合）。
P1 计划：Projectile（视觉投射）、Shield（护盾表现）、Trail（拖尾）。
P2 计划：Portal（传送门表现）、World（世界动态VFX）。

非 Composite Definition 必须提供 Niagara System（Niagara系统）；Composite 通过 Steps（子步骤）组合其他 Definition，支持参数覆盖和延迟。
