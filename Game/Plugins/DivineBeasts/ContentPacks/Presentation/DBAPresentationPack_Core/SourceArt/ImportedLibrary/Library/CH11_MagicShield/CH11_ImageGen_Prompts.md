# CH11 MagicShield 生成提示词与技术约束

## ImageGen 视觉母版

- 规则护盾：正视圆形魔法护盾，清晰六边形单元，冰蓝青白核心，少量紫电，AAA fantasy MMORPG 与科技魔法融合，至少 12% 留白，移动端可读，无文字与场景。
- 破裂护盾：正视破碎圆形护盾，大块碎片与放射裂纹，冰蓝青白高光和少量紫电，中心爆点清楚，避免微小碎屑与 1px 裂纹。

## 最终运行纹理约束

- 1024x1024、2 次幂、mobile-friendly、mip-safe、compression-safe。
- Hex、Ripple、Broken 输出 RGBA；四角 Alpha=0，透明区 RGB=0，主体四周至少 10% 留白。
- Noise、Mask、Dissolve、Distortion、FresnelBreak 输出单通道 G8，sRGB 关闭。
- 所有 Noise、Dissolve、Distortion 与环状边缘数据必须四边可平铺或边界归零。
- 无文字、无水印、无边框、无 UI、无场景、无摄影透视、无 AI 粒噪。
