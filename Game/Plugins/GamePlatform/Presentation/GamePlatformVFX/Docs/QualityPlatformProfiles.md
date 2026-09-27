# QualityPlatformProfiles（质量与平台配置）

Definition支持PlatformVariants和QualityVariants，选择顺序为Platform → Quality → Default。PC/Android主要通过Niagara Effect Type、System overrides和Device Profiles共同实现质量降级。

低质量可减少粒子、禁用Renderer/Light、使用便宜Mesh/Material或选择fallback System，但不能改变Gameplay真实尺寸、命中范围和权威时序。

平台变体Key为中立平台名，不写入项目英雄或地图业务。