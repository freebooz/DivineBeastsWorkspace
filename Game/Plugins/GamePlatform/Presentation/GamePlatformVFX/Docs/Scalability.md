# Scalability（性能伸缩）

两层性能控制：
1. GamePlatformVFXScalabilityPolicy（业务伸缩策略）：按 Critical/Combat/Status/Ambient 重要度和世界活动实例数决定是否允许生成。
2. Niagara Effect Type（Niagara特效类型）：由资产侧执行距离、并发、显著性和质量档控制。

Pooling（池化）优先使用 Niagara 原生池。Critical（关键）效果不因通用实例数量上限被拒绝。

当前 MaxActiveInstances 默认 256，可通过 DefaultGamePlatformVFX.ini 调整；最终数值必须根据目标平台 CPU/GPU/Profile（性能分析）结果确定。
