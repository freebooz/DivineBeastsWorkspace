# SSLM新手村薄雾与月光调整

用户先要求增加新手村雾，随后明确“光照调整暗一些，月光照明”。最终按月夜方向执行，覆盖该地图在配色V3规范中的默认晨光方向；不修改其他世界的视觉预设。

归属：`DBAWorldPack_Village/Content/Maps/L_Village_Start.umap`。通过Monolith MCP 0.23.0与锁定UE5.8编辑器原位保存；复用引擎原生渲染，不增加平台类型、客户端播放器、Niagara System、材质、天气权威或日夜循环。平台天气复制和Surface既有链路保持原职责。

| 对象 | 最终值与用途 |
| --- | --- |
| 既有DirectionalLight | 强度0.85，颜色sRGB BE D6 F2，方向Pitch -32/Yaw -35，光源角1.2；冷色侧上方月光 |
| 既有SkyLight | 强度0.18，维持原生实时捕获；提供克制环境填光 |
| VillageMoonlightExposure | 一个地图内PostProcessVolume，Manual固定曝光、关闭物理相机曝光、Bias 2.3、Bloom 0.1，防止自动曝光把夜景抬亮 |
| 既有高度雾 | Density 0.008、Falloff 0.18、StartDistance 1200厘米、MaxOpacity 0.32；第二层Density 0.006/Falloff 1.25/Offset 50厘米 |
| 三处LocalFogVolume | 半径40/50米，位于出生区外围；Radial 0.025、Height 0.32、HeightFalloff 2000；不叠加全屏粒子 |
| 雾颜色 | 高度雾sRGB 485D73，局部散射反照率sRGB CBD8E6，显式IEC sRGB至线性转换；不改地形材质或套全屏色调滤镜 |

体积雾关闭，局部雾使用原生有界Volume；这些是成本控制选择，不是GPU达标证明。未新增动态灯光和粒子；没有在Dedicated Server上实现雾播放器。现有两个PlayerStart的变换与导航、地形保持原样。

实现与只读核验：`Tools/Unreal/World/AuthorVillageMistUE.py`、`AuthorVillageMoonlightUE.py`、`Tests/Assets/ValidateVillageMistUE.py`。保存前备份原地图及外置数据，均位于`Saved/Validation/VillageMist_20261018/Backups`。早期较暗曝光和晨雾采样作为历史过程保留，最终值以MoonlightAuthoring、ColdValidation和最终截图为准。

独立UE进程PID66448、退出0，重载后校验唯一主光/天光、固定曝光、一处高度雾/三处局部雾、两个出生点及未保存临时审查Actor；地图SHA为`16232e118ac71d7a148a2e78a596888e29d9cb6504841232ac3846852a1503d3`。临时人形审查代理已销毁，不进入地图或Cook。

原生截图：`VillageMoonlight_GameAngle.png`及`VillageMoonlight_Depth.png`，1600×900，无后期。前后游戏视角统一位置(-1400,-1400,2300)、看向(900,900,0)。地面中心ROI的线性亮度均值从0.48265降至0.16774，约为原来的34.75%，该ROI近黑像素比例为0。这个数值仅描述当前地面截图，不证明运行中所有角色和预警都已人工验收。当前编辑器关卡只包含已保存地形与环境，截图不代表完整运行时村庄生成或联机。

证据根`Saved/Validation/VillageMist_20261018`：Authoring/MoonlightAuthoring记录实际参数，ColdValidation/ColdProcess记录独立重载，VisualEvidence记录原生截图哈希与亮度采样；最终Cook/Stage结果另归FoundationM0对应独占RunId。未启动新的前端联机或重启现有服务。人工夜景可读性、不同天气/画质和多人GPU仍待验证。

最终发布验证：Client Cook/Stage RunId `bcedea74-44ee-4c07-99aa-1a679cded413`通过，真实容器629项既有必需资源完整；Server RunId `63b1cfd1-02ca-4b0b-8578-f0f6582b8ea8`通过，表现剥离审计ForbiddenEntries=0。定向飞弹1号/治疗5号的85项权威包、12项UIProfile与10项运行VFX包也在同一最终容器核对通过。以上是独立验证包，不代表已经部署到正在运行的Village/MainArena实例。
