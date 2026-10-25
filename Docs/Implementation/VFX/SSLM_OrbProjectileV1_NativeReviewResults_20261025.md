# 《神兽联盟》球形火焰／厚冰块 V1｜UE5.8 原生审核结果

日期：2026-10-25。工作空间：DivineBeastsWorkspace。所有新增正式目标资产均仅位于 /DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1 审核路径。没有修改GAS（游戏技能系统）、服务器发射时序、命中判定、碰撞、正式表现绑定或主分支。

## 一、目标审核资产已创建并冷加载编译

| 阶段 | Niagara系统 | 发射器数量 | UE5.8冷重载编译 |
|---|---|---:|---|
| F1 大火球成形 | NS_DBA_FireOrb_Form_Review | 5 | 0错误、0警告 |
| I1 厚冰块成形 | NS_DBA_IceBlock_Form_Review | 5 | 0错误、0警告 |
| F2 火球飞行/拖尾 | NS_DBA_FireOrb_FlightTrail_Review | 5 | 0错误、0警告 |
| I2 冰块飞行/拖尾 | NS_DBA_IceBlock_FlightTrail_Review | 5 | 0错误、0警告 |

已新增公共视觉模块 NM_DBA_OrbVisualMotion_Review（分层外观运动）与 NM_DBA_OrbTrailFade_Review（拖尾宽度/透明度）。20组真实发射器已创建。冰块成形与飞行真实使用独立厚块多面体 SM_DBA_IceBlock_FacetedCore_Review（静态网格），其来源不是原细长冰箭。两个飞行系统各含一个真实 Ribbon Renderer（带状粒子渲染器），均设置世界空间。冷重载网格和渲染器核验见 Saved/Validation/OrbProjectileV1/TargetNiagaraColdLoad.json。

## 二、真实PIE（编辑器内游戏运行）审核画面

全部为1280×720原生 SceneCapture（场景捕获），独立临时审核地图，不是AI概念图或真实联网对战。视觉截图存在但人工审核 **暂未通过**。

| 阶段 | 实际截图路径 | 观测结果 |
|---|---|---|
| F1 成形 | Saved/Validation/OrbProjectileV1/NativePIE/20261024T235405Z/F1_OrbFormed.png | 火焰明显可见，但呈密集尖锐火舌簇，偏向模型肩背，不是高质量三维大火球 |
| I1 成形 | Saved/Validation/OrbProjectileV1/NativePIE/20261024T235501Z/I1_IceFormed.png | 真实厚冰块可见，然而外形偏方盒、遮挡头胸，视觉锚点需移至双掌中点 |
| F2 火球飞行 | Saved/Validation/OrbProjectileV1/NativeFlightPIE/20261025T000420Z/F2_TrailPeak.png | 可见燃烧球体，但没有可辨识的连续Ribbon尾迹 |
| I2 冰块飞行 | Saved/Validation/OrbProjectileV1/NativeFlightPIE/20261024T235712Z/I2_TrailPeak.png | 可见多面冰块和微弱蓝色粒子，但尚无合格的连续Ribbon尾迹 |

飞行画面为审查NiagaraActor沿脚本轨迹移动的隔离实验，不构成对权威弹体同步/网络纠正的验收。每个阶段另有四帧原生截图和Result.json（运行结果）。

## 三、真实Ribbon模拟缓存发现

已将F2、I2 Ribbon的 SpawnRate（粒子生成率）设为65/秒、Emitter（发射器）无限循环，保存且原生编译无报错。

F2第65模拟步的 UE5.8 NiagaraSimCache（粒子模拟缓存）证实：
- PrimaryFireRibbon（主火焰尾迹）存在18个粒子和18个不同世界坐标。
- RibbonWidth（尾迹宽度）0.91至23.66厘米；Alpha（透明度）0.0067至0.78。
- 粒子Position（位置）X范围1.77–66.11厘米、Z范围166.94–176.81厘米。
- 其他发射器：TravelCore 1个、FlightFlames 14个、SecondaryWisps 7个、TrailingEmbers 9个。
- 源证据：Saved/Validation/OrbProjectileV1/NativeFlightPIESimCache/20261025T000935Z/Result.json。

这证明尾迹粒子有位置、颜色和宽度，并不能证明Ribbon几何连接和材质渲染正确。优先排查 RibbonLinkOrder（链接排序）与 RibbonID（Ribbon分组）的生成、UV、Ribbon Renderer绑定及场景可见性，不宜再盲目提高粒子率。

材质只读核查：火尾审核实例P_Opacity约0.82、P_Intensity为9，冰尾OpacityScale约0.65、EmissiveStrength约2.4，当前未发现简单参数为零。明亮火尾可能还需要降低泛光并优化贴图。

## 四、风险与下一步门禁

1. F1需更圆润、更厚实的白金核心与金橙球面，并绑定真实双掌中点；当前火焰位置及形态未通过。
2. I1需对真实多面体冰块加强不规则晶面、裂纹/高光/折射控制，移开人物头胸。
3. F2/I2必须补齐连续且沿世界历史位置的真正可视Ribbon，并检查急转、瞬移断尾和生命周期清理。
4. 两种飞行均尚未接入真实AGamePlatformCombatProjectile（权威平台弹体），未验证多人联网、四档画质、CPU/GPU性能和真实连续视频。
5. 禁止将当前审核资源替换正式技能；未经人工签收不做正式资源合并或Git提交。

结论：**原生目标资产和20发射器已创建、冷重载编译通过、主体部分可见；连续拖尾与美术质量未通过人工审核。**
