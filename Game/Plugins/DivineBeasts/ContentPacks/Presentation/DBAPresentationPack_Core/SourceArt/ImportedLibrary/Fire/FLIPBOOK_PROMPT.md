# 原创火苗动画图集

使用内置 ImageGen 生成，源图保留原始 RGBA 通道，未进行外部图像编辑。输出保存为 `Textures/T_PY_FlameFlipbook.png`。实际源尺寸为 1254×1254，16 帧；2048 是导入后的二次幂运行上限，不是原生生成分辨率。

## 生成提示词

Create an original production-ready VFX flipbook sprite atlas, exactly 4 columns by 4 rows, 16 equal square cells, seamless chronological animation read left-to-right then top-to-bottom. Square 2048x2048 RGBA canvas if available. Each cell is one successive frame of THE SAME compact vertical fire plume, not different fire designs. Looping turbulent flame simulation over one second: curls roll upward, tongues split and rejoin with coherent motion, last frame flows into first. The plume is upright with narrow base at bottom center, wider luminous turbulent middle and thin licking tips upward. Photorealistic cinematic gaseous flames with rich fine wispy detail, golden amber mids, hot pale-yellow tiny center, deep burnt orange translucent fringes. Internal dark gaps, varied density, no huge solid white blobs, no heavy bloom. Real alpha transparency everywhere outside flame. All sixteen cells must have identical camera, scale, baseline and center, 12 percent transparent gutters INSIDE EACH CELL; no flame may cross a cell boundary. No lines, no grid borders, no text, no numbers, no props, no character, no room, no floor, no background color. Perfectly registered equal-sized tile grid covering the complete canvas. Intended for an animated Unreal Niagara particle material with subimage frame interpolation. High frequency physically convincing flame rolls, not a drawn icon or orange smoke.

## 接入

`build_flipbook_material.py` 创建 ParticleSubUV 材质和实例。`Modules/FireMotionFlipbook.hlsl` 写入连续 SubImageIndex。所有使用图集的精灵设置 SubImageSize=(4,4)、bSubImageBlend=true，材质 ParticleSubUV.blend=true。ParticleIndex 绑定 Particles.UniqueID。

运行检查覆盖帧号范围 [0,16)、分数帧、错开相位、时间推进、粒子分布与生命周期。
