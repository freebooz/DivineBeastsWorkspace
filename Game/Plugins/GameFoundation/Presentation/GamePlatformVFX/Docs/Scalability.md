# Scalability（性能与画质）

`FGamePlatformVFXScalabilityPolicy` 目前只做实例数量级的入口保护。

正式项目应主要依赖 Niagara 原生：

- Effect Type（特效类型）
- Significance（重要性）
- Distance Culling（距离裁剪）
- Instance Count Culling（实例数量裁剪）
- Platform / Quality Scalability（平台/画质可伸缩规则）
- Niagara Component Pool（组件池）

建议 Effect Type：

- `FXT_GPVFX_Critical`
- `FXT_GPVFX_Combat`
- `FXT_GPVFX_Status`
- `FXT_GPVFX_Ambient`
