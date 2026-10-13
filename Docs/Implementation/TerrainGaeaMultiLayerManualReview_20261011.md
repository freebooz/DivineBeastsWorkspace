# 《SSLM》Gaea多层地形与PCG人工验收单

版本：2026-10-11。当前状态：**待真人签收**。自动化资源生成、源码编译或UE资产存在均不能代替可见画面审查。

## 进入审核前的技术门禁

1. 使用真实Gaea 2.3出口（不是程序化校准样本）执行高度/掩码SHA256、坐标、方向和分辨率验证；依TerrainMask Data Contract V1导入。
2. 真UE LayerInfo 10个Name严格对应LS_Grass、LS_ForestSoil、LS_Dirt、LS_Rock、LS_Scree、LS_Sand、LS_Gravel、LS_RoadDirt、LS_FarmSoil、LS_ShoreMud。Landscape 1009×1009 / 64组件或经批准的等效地形真实保存、独立编辑器进程重启可加载。
3. 真实UE Material10层Shader无错误；植被/道路/湖岸场景至少三地貌层可见无权重黑洞、法线反转或拉伸。
4. PCG Graph需要使用与Landscape同源图层权重，按已经登记的Biome、Layer、PriorityCarve生效；已保存的Realized Graph及Blueprint不可用占位或未绑定的PCGComponent宣称运行成功。
5. 客户端和服务器干净独立Cook、UnrealPak审查：服务器只包含必要权威几何及低分辨安全材质，不携带纯装饰VFX、表面贴图及测试审查地图。

## 人工逐项检查

- 地形轮廓：湖心三岛、环形悬崖、河道及道路自然，具有魔兽世界式明确远景轮廓，但材质现代精细、不过分锐化/颗粒。
- 材质层：草地、森林腐殖土、普通泥、岩石、碎石、沙、砾石、压实土路、农田、湿泥之间自然过渡；坡度陡峭时裸岩合理，水岸湿泥不在干燥山顶大面积出现。
- Gaea坐标：原Gaea世界到UE高程、Mask精确对齐，Tile边界不出现裂纹或平铺十字边，翻转方向正确。
- PCG场景：森林生成、道路切林、农田切除、湖岸植被、坡面碎石、人工锁定、桥与围栏排除符合已有G01-G16合同；错误输入/缺Asset不会造成半次生成或服务器权威变化。
- 天气：雨后湿润、雪层坡度和海拔、落叶/桃花覆盖与基础材质一致，不卡帧、不在世界切换后残留MID或VFX。
- 性能：对目标PC高/中/低三档记录真实材质Shader指令、纹理采样数、GPU ms/FPS、CPU PCG Bake ms、实例数、客户端/服务器内存峰值及Cook大小；没有批准预算前禁止编造“低于X ms”的达标结论。
- 人工签收人、设备型号、日期、UE版本、Gaea版本、正式图片编号、缺陷追踪及最终同意/拒绝必须由真实审核者填写。

十份Landscape LayerInfo已全部创建并经过独立UE5.8进程反射验收。SourceArt、4份Blueprint及1份Graph仍属于技术QA；尚无真实十层Landscape审核地图、PCG Bake或人工可见验证画面，因此**仍不能宣称最终人工审美验收通过**。
