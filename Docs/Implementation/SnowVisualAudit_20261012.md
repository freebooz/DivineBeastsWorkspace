# SSLM写实积雪视觉审查（2026-10-12）

## 本轮实际查看的UE Editor原始截图

- Game/Saved/Screenshots/SnowReview/SnowReview_Heavy_20261012.png：HeavySnow=0.85。
- Game/Saved/Screenshots/SnowReview/SnowReview_Light_20261012.png：LightSnow=0.23。
- Game/Saved/Screenshots/SnowReview/SnowReview_Heavy_vs_Light_20261012.png：并排对照，下半部大面积黑边。

两张单独截图均能观察到地表表面随雪量改变：大雪时高频亮点密集，小雪时主要是模糊的宏观斑块。然而地表整体偏灰粉紫，缺少冷白雪面与真实PBR体积层次；墙体、屋顶代理仍显橙色/暗色，未清楚显示雪层在不同坡度的自然积累；截图里看不出近景/远景雪花运动、贴地雪粉以及地形碰撞。故不能认定实现了参考视频的最终真实雪视觉效果。

## 当前已有工程成果与证据限制

- 已有56表达式雪母材质M_GP_SnowCover_Detailed、项目MI_DBA_Snow_Detailed、详细降雪Niagara、贴地雪粉、雪面法线/ORM/遮罩等真实UE资产。
- 已有Village材质桥接、SnowAmount与Texture2D的MID回读、Client/Server Cook及双客户端基础重进的历史通过记录。
- 但上述自动化证据不等于这次截图的视觉通过，更不等于GPU、风场、室内遮挡和完整天气过渡已验收。本轮没有看到与上述三张截屏配套的GPU/帧时、无遮挡雪粉动态视频或晴天对照图。

## 按优先级继续修复与验收

1. 在独立SnowReview测试地图保存中性日光对照配置：检测SkyLight、DirectionalLight、ExponentialHeightFog、PostProcess曝光/白平衡，避免当前粉紫色调掩盖PBR真实问题。不修改正式Village共享图。
2. 确认岩石、坡面、墙顶和地面都正确继承项目雪材质实例；截取25度、50度及垂直面的材质覆盖规律，禁止只看一大片平地。
3. 调查大雪出现的高频亮点“砂纸”观感，复核Normal强度、MicroHeight平铺、Roughness和SnowTint；保留雪量0/0.23/0.85的数值门禁。
4. 在真实PIE环境验收NE_GP_Snow_Near、Far和GroundDrift的可见性、随相机生成、雪粉地面触碰与真实风向。已有发射器编译通过不代表肉眼可见。
5. 同一机位捕获Clear、LightSnow、HeavySnow三张全幅1280×720以上无遮挡图片，修复对比拼图下半部黑边；另提供正式Village大雪可见画面。
6. 最终才记录实际GPU帧时、透明粒子Overdraw、Client/Server天气联机同步、人工视觉签收。

本轮为只读真实截图视觉检查和问题归档，未修改天气材质、关卡、Niagara及其他并行PCG/MOBA代码，不宣称完成视觉改造或人工签收。
