# BehaviorModel（VFX行为模型）

平台保留 Instant、Attached、Projectile、Beam、Area、Shield、Portal、Trail、World、Composite 十个 Behavior 名称，但它们不是十套 C++ 执行器。

正式原则：

- 非 Composite：统一由 Generic Niagara Executor（通用 Niagara 执行器）处理；
- Composite：唯一具有独立时序编排逻辑；
- Attached：通用执行器可消费 Definition 的默认 AttachPoint；
- Beam：通用执行器把 Source/Target 中立位置写入 Definition 声明的 Niagara Position 参数；
- Area：通用执行器可写入默认 Radius 参数；
- Projectile/Shield/Portal/Trail/World/Instant：主要用于内容制作、审核和语义分类，具体运动/阶段/外观应尽量由 Niagara System 自身完成。

这些 Behavior 全部是纯表现语义，不拥有碰撞、伤害、Buff、护盾数值、Ticket、Travel 或任何 Gameplay 权威状态。