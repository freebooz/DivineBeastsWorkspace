# CH24 Combat Readability ImageGen 补充提示词

## 全章硬约束

- 单张 PNG，默认 1024×1024；目标标记为 512×512；全部为 2 次幂。
- 纯黑背景 `#000000`，白/灰数据，不使用 Alpha，不烘焙最终颜色。
- 正交俯视、中心对齐、几何边界明确；画布四周至少保留 10% 黑色安全区。
- 关键线宽不得依赖 1px 细线；必须兼容 ASTC/ETC、Mip 与移动端缩放。
- 无文字、数字、Logo、水印、边框、UI 面板、场景、地面、透视、摄影物体。

## 战斗区域母图

AAA fantasy MMORPG combat telegraph design reference, top-down orthographic, clean hostile danger circle, donut, sector, line, cross, cone, countdown wedges and direction arrows, sharp scarlet and orange-gold energy outlines on pure black, clear hierarchy, symmetric, large compression-safe features, no micro-noise, production-ready, 1024x1024, power-of-two, mobile-friendly, mip-safe, compression-safe, clean edges.

## 目标与友方区域母图

AAA fantasy MMORPG target marker and friendly-area design reference, top-down orthographic, cyan friendly ring, healing rune, concentric target reticle, eight-spike boss target, faceted critical starburst, pure black negative space, symmetric, readable at small size, large compression-safe features, production-ready, 1024x1024, mobile-friendly, mip-safe, clean edges.

## 统一负面词

no text, no letters, no numbers, no logo, no watermark, no border, no UI panel, no scene, no floor, no camera perspective, no photo object, no unnecessary background detail, no jpeg artifacts, no thin 1px lines, no AI grain, no edge contamination
