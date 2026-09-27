# BehaviorModel（10种VFX行为模型）

统一支持 Instant、Attached、Projectile、Beam、Area、Shield、Portal、Trail、World、Composite 十种Behavior。差异由Definition和内部策略表达，不拆十个插件、不建十套Pool、不为每Definition生成Actor类。

默认直接使用Niagara Component；Projectile/Area/Shield/Portal等名称只描述视觉行为，不获得碰撞、伤害、数值、Ticket或Travel权威。