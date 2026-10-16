# SSLM 寒冰箭 A「玄霜棱矢」候选制作记录

## 验收结论

用户要求真实 VFX 与已选 A 概念图视觉高度一致。本轮产物是可渲染的审核候选，**尚未达到该视觉验收标准，不得发布为最终技能效果**。三阶段能运行、材质能编译、视频能生成，均不等同于视觉合格。

| 状态 | 本轮证据与边界 |
|---|---|
| 自动制作 | 已建立施法、飞行、命中三个独立候选系统及实际纹理、材质、网格；尚未完成正式配置与四档画质合同 |
| 编辑器验证 | 三系统最新编译错误为 0；五个实际使用的母材质编译成功；已保存 68 帧连续原生截图和 MP4；仍有一条经过实际模拟核查的 Ribbon 启发式警告 |
| 游戏运行验证 | 未完成角色挂点、真实技能事件、取消/预测回滚、多人联网、四档负载、客户端与服务器 Cook 验证。Village 图中截图属于编辑器现有场景验证，不是游戏运行通过 |
| 人工视觉验收 | 未完成；没有用户对本轮实机候选的视觉签收 |

本轮仍未提供已经确认的第二轮分镜与第三轮独立素材设计稿。现有 A 概念图是唯一已选设计依据，不能把制作时序夹具称为已确认分镜。

## 当前视觉差异与下一轮重点

1. **主体和核心：** 已有闭合晶体、切面响应、独立冰内裂隙纹理；但大切面的块面感、内部光核的几何感仍明显，未达到原图的通透破裂冰晶质感。下一轮应优先重构晶体边缘与内部折射/高光关系，不增加外围杂粒掩盖问题。
2. **轨迹：** 已有两条近身 Mesh 流带及世界空间 Ribbon；历史位置和年龄衰减有效，但近身流带仍显厚、纹理/几何起伏偏规整，光核与尾迹连接还不够自然。
3. **命中：** 碎晶、薄环、闪光与冰雾已有独立发射器，能连续消散；碎晶分布、冰雾体积和瞬间能量密度仍需对照 A 图调整。

施法法阵保留可复用透明遮罩，在攻击方向垂直平面凝聚。固定侧视机位会显著压扁该平面，斜视机位用于检查体积；不得用调整相机隐藏真实游戏镜头下的可读性问题。

## 实际技术修复

- Monolith 0.23.0 及 13 个依赖插件共 64 个模块已在隔离宿主真实编译，备份原 DLL/模块清单后恢复了正式 UE5.8 编辑器连接；未手改 BuildId。恢复记录在 `Saved/Validation/IceArrow/Production_20261016/DependencyRecoveryManifest.json`。
- 原有 HLSL 中 `Index%128` 未正确替换输入符号，真实 GPU 着色器报错；候选改为 `Index % 128`。旧正式模块未覆盖。
- 同一具有参数映射副作用的 Custom HLSL 同时放在 Spawn/Update 时，生成函数写入 `MapSpawn`，更新位置/颜色没有进入 `MapUpdate`。候选只在 Update 执行运动，Spawn 使用原生 InitializeParticle。已有真实截图证明流带恢复。
- Monolith 的 `set_curve_value` 把曲线文本写进普通 float 输入，在本例产生 0 值。候选改用 FloatFromCurve 动态输入、真实曲线 DI 和 NormalizedAge 绑定；没有继续使用该错误结果。
- `HistoryTrailSimCache.json` 记录真实 31 个 CPU 粒子：宽度约 0.189—7.808 cm，Alpha 约 0.017—0.703，历史 X 坐标约 -307.27—-19.27 cm。当前 `RendererAttributeInit` 来自按模块路径识别的静态启发式；警告保留，不冒称“零警告”。
- 新增纯视觉 `User.StopFlight` 参数在候选中终止网格粒子，历史轨迹停止发射后自然排空。连续夹具已观察命中衔接；**该参数尚未接入正式 GamePlatformVFX 停止流程**，不能因此宣称真实技能取消已通过。

## 候选资源与制作配置

根挂载点为 `/DBAPresentationPack_Core/VFX/IceArrow`。所有本轮新增原生资产位于各用途的 `Review` 子目录；正式 Definition、Catalog 与服务器技能规则没有改动。

| 系统 | 发射器 | 实现与关键配置 |
|---|---|---|
| `Niagara/Review/NS_DBA_IceArrow_Cast_A` | GatherCrystal、GatherCore、CastingRune、GatherFragments | 4 个 GPU 发射器；1 个晶体、1 个内核、1 个竖直法阵、5 个碎片；制作寿命 1 秒 |
| `Niagara/Review/NS_DBA_IceArrow_Flight_A` | CrystalArrow、EnergyCore、FacetPlates、FrostFlow、FrostShards | 5 个 GPU Mesh 发射器；分别 1/1/5/2/7 粒子；制作寿命 3 秒；停止参数只控制外观 |
| 同上 | HistoryTrail | CPU Ribbon，75 个/秒，寿命 0.42 秒，最大宽度 8 cm；宽度和透明度按年龄曲线递减 |
| `Niagara/Review/NS_DBA_IceArrow_Impact_A` | BurstCrystals、Shockwave、IceMist、ContactFlash | 4 个 GPU 发射器；14 Mesh 碎晶/1 Mesh 薄环/8 Sprite 雾/1 Sprite 接触闪光；寿命分别 0.85/0.45/0.95/0.15 秒 |

最新原生配置以 `Cast_FinalCandidateSpec.json`、`Flight_FinalCandidateSpec.json`、`Impact_FinalCandidateSpec.json` 为准。原生资源索引另存 `CandidateResourceIndex.json`。旧 A/A2 模型和试验遮罩作为可回溯候选保留，最终 Cook 取舍尚未完成。

`SM_DBA_IceArrow_CrystalA2` 为闭合低面数晶体：42 顶点、80 三角形源模型，正 X 为攻击方向。实际 UV、法线和源 OBJ 经 UE 导入；没有把概念图贴到整张技能平面上。

独立新纹理 `T_DBA_IceArrow_InternalFracture_A` 为内置 imagegen 生成的黑白冰内结构遮罩；原图保存于内容包 `SourceArt/VFX/IceArrow/Review/`。这是不需要透明背景的线性 R 遮罩，sRGB=false、Masks 压缩、最大运行尺寸 1024；不是透明 Sprite。原有透明素材保持自己的 Alpha，不把底色烘进透明纹理。生成提示词随源图保存。

## 性能与验证边界

- 当前只记录结构与着色器统计，不宣称完成 GPU/CPU 毫秒性能测试。晶体母材质约 1247 pixel / 344 vertex 指令；能量与薄环约 153 / 278。指令数不能直接换算成场景 GPU 毫秒。
- 曾测试逐像素透明照明，晶体 pixel 指令由 1234 增至 1885，视觉收益不足，已撤回。
- 当前候选尚未完成 Effect Type、四档 QualityVariants、最低画质保底、5v5 并发、NDC 共享命中和独立实例成本核验。不能引用全局候选预算宣称本技能已经达标。
- 实机视频为 1280×720、24fps、68 帧；编码时长约 2.833 秒。这不是 24 FPS 实时性能测试，帧数来自固定模拟步长逐帧截图。
- 本轮复核原始 60 项资源 SHA-256，变化数量为 0。19 个候选非地图资源包已在编辑器内原生重载成功，另有 1 个原生审核地图。此结果不代表关闭编辑器后的进程重启、客户端 Cook 或服务端剥离验证已经通过。

## 证据路径

均在工作空间 `Saved/Validation/IceArrow/Production_20261016/` 下：

- `IceArrow_A_EditorPreview.mp4`：真实连续预览。
- `SequenceA/Frame_0014.png`、`Frame_0030.png`、`Frame_0036.png`、`Frame_0043.png`、`Frame_0067.png`：凝聚、飞行、接触、扩散、结束关键帧。
- `Flight_VillageSceneA.png`：现有 Village 地图中的编辑器渲染；该地图目前仅有少量基础 Actor，不是完整美术场景。
- `CandidateValidationSummary.json`：最新三系统、五母材质、原始资源散列复核。
- `HistoryTrailSimCache.json`：实际轨迹粒子数据。
- `SequenceA/SequenceReport.json`：本次渲染事件与状态。

专用审核地图：`/DBAPresentationPack_Core/VFX/IceArrow/Maps/Review/L_DBA_IceArrow_A_Review`。连续渲染由 `Tools/Unreal/VFX/DevelopmentMagic/ReviewIceArrowSequenceA.py` 在专属编辑器中复现；它是审核工具，不是第二套正式 VFX 播放器。
