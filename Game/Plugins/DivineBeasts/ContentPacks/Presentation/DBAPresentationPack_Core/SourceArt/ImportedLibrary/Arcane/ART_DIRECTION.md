# 奥术飞弹视觉重制

原版问题：不透明白色菱形亮芯、细线拖尾，蓄力和命中缺少流动与体积。

新版：蓝色内部流线包裹在紫色烟焰中，尖端朝向运动方向，尾部柔软翻卷。主图不靠纯白过曝表现能量；交织细丝穿过较宽的紫色尾迹。掌心持续聚能，命中形成六层快速扩散的紫焰、放射碎光和符文涟漪。

原创主纹理 `Textures/T_AM_ArcanePlasma.png` 使用内置 ImageGen 生成，保留原始透明通道。美术生成要求为：透明背景、竖直奥术等离子彗星，尖端在上，细长尾焰向下；青蓝色亮丝、深紫色流动烟焰、丰富内部细节，无环境、文字或角色。没有使用魔兽世界提取资产。当前运行资产通过 Monolith 导入到 `/Game/ArcaneMage/Textures/T_AM_ArcanePlasma`，最大 1024。

材质重建脚本 `remake_materials.py` 保存 UV 动态扰动、能量参数以及尾迹的连续交织函数。飞弹已移除不透明亮芯网格的渲染器，旧源网格保留供历史对照。新的运动源代码在 `Modules/ArcaneMotionCinematic.hlsl`、`WakeSpawn.hlsl` 和 `WakeFade.hlsl`。

验收以实际引擎连续画面、运行时粒子数量、有限生命周期和场景性能测量为准。测试素材位于 `Saved/VFXValidation/ArcaneMage`。
