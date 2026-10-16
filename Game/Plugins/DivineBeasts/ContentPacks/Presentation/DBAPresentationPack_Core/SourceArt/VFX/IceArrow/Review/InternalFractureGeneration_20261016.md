# 独立冰内裂隙遮罩来源

用途：真实三维冰晶材质的线性 R 通道微结构遮罩；无透明背景要求，不是完整技能图。来源：内置 imagegen；保存原输出后经 Monolith 原生导入，不使用 API/CLI 回退。

源文件：`T_DBA_IceArrow_InternalFracture_A.png`。实际运行设置为 sRGB=false、Masks、最大尺寸1024，保留原源图。

## 最终生成提示词

Generate a single square seamless grayscale technical texture for a AAA realtime ice crystal material, 1024x1024. This is ONLY a flat UV surface microstructure mask, never a finished object or scene. Black to dark gray clear ice matrix with sparse razor-thin branching white internal fracture slivers, irregular overlapping angular clear ice lamellae at varied scales, hairline glints, subtle cloudy inclusions in a few small areas. Most area dark, restrained high frequency crystalline detail. Some medium slanted fractured plates but NO regular polygon grid, no Voronoi/cobblestone pattern, no thick glowing outlines, no snowflakes, no gems or arrows, no object silhouettes, no volume sphere, no background scenery, no frame, no text, no bloom, no baked colored lighting. Flat orthographic uniform map filling canvas edge to edge, edges tile plausibly. Intended as a linear sampled scalar mask on independently modeled three dimensional ice; high-end convincing clear broken ice internal structure. Opaque black technical mask background is intentional; no transparency needed.

检查边界：实际源图尺寸以PNG头为准，生成提示中的1024不是尺寸验证。没有证明无缝边界误差为零；当前UV用于有限晶体表面，仍需最终美术审核。
