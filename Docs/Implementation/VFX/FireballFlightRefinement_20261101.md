# 标准火球参考图优化执行计划

2026-11-01最新调优：用户确认音乐及施法声正常，要求加长拖尾、球体直径0.4米、材质发光减半。唯一尾迹由400cm加长至700cm，核心40cm；外焰57.14cm、柔光74.29cm、原始尾根137.14cm等比缩小，保留饱满衔接。全部火球自有发光材质按统一0.5系数写绝对值；无标量的共享柔光由本火球ParticleColor应用该系数，不修改共享母材质。年龄Alpha延迟至最后35%寿命衰减，空间及尾端渐隐继续生效。最大年龄及平台Drain仍0.5秒。以下旧400cm/70cm段落作为上一轮记录，本轮以此段和PresentationManifest为准。

本轮步骤：备份当前五个受影响Niagara包；通过自有Monolith原位同步模块并编译保存三阶段；执行当前速度1800cm/s及高速、网络阶跃、四画质、转向、Drain和静止的RHI/SimCache；完整Editor输入证明与客户端Cook更新；核对现行服务器兼容性再更新两个客户端，提供原生动态帧人工审核。不修改权威弹道、音频或另一任务的传送门实现。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans，在当前正式工程串行实施；最后一次独立只读审查。每项按复现失败、最小修复、独立验证推进。

**Goal:** 以用户最新火球参考图为视觉标准，实现轮廓稳定的明亮三维圆球、连续金色包裹层与宽幅交织尾焰，过程提供纹理和材质人工审核图，完成同步双端打包与两窗实机核验。

**Architecture:** 既有三阶段 System、Definition、Catalog、GamePlatformCombat 权威投射物及 GamePlatformVFX 唯一执行链继续复用；Flight 原位维护一个 System、七个 Emitter。纹理/材质由同一作者写入，Niagara 作者只负责组合，所有作者、验证器和 Cook 读取同一 PresentationManifest；服务器不承担 Niagara 表现。

**Tech Stack:** 锁定 UE5.8、Monolith MCP、Niagara、原生 Material/Texture2D、C++ Automation、PowerShell 增量构建；绘制类纹理使用内置 imagegen，数值数据纹理由 UE 原生确定性烘焙。

**Spec:** [用户粘贴方案](../../References/UserProvided/FireballFlight_ReferencePlan_20261101.txt)；形状参考为 `Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/SpellProjectiles/Fireball/Reference_FlightShape.png`。后续直接指令优先：按最佳方案采用完整三维圆球；实施过程中提供纹理、材质人工审核效果图。

## Global Constraints

- 当前工程 `Game/DivineBeastsArena.uproject`，资源归 `DBAPresentationPack_Core`；不采用参考方案的另一棵 `/Game/DivineBeasts/VFX/...` 工程树。
- 飞行 System 保持 `/DBAPresentationPack_Core/VFX/SpellProjectiles/Niagara/NS_DBA_FireballFlight`；Cast/Impact 维持当前稳定身份，不另建 Blueprint Projectile、播放器、池或预算真源。
- 真实 .uasset/.umap 只能由 UE 原生工具创建、修改、编译、保存、重载；Codex 禁用 HWebCodex。
- 所有生成、烘焙、临时、Shader、Stage、打包、审核证据均在当前工程内，使用动态根路径与 Saved；源图最终落入项目 SourceArt。
- 原生资产修改串行；保留已存在手工修改。共享 main 不自动提交、重置或复制插件；本计划不创建另一份工程。
- 固定 Win64/Development、`-NoSharedPCH -NoUBA -MaxParallelActions 8`；Editor 另用 `-UsePrecompiled -AdditionalPlugins DBAServer`，Client/Server 不用 UsePrecompiled。
- 独立客户端调试 `t.MaxFPS 0`、`r.VSync 0`，记录实际生效值，不覆盖玩家持久偏好或显卡全局设置。
- 每次前端验证先绑定真实 Client、Village、MainArena 的输入、完整构建、Cook/Stage、EXE、PID、启动身份和复制合同；保留网络校验，不重置账号/角色数据。
- 图片审核、源码回归、原生资产、构建、Cook、进程就绪、真实准入、人工签收与 GPU 实测分别报告。

## Review Focus

1. 侧面、斜侧、迎面和自动曝光变化时，内核仍为圆球；外焰不能叠成星形、硬壳或平面光斑（任务3、5）。
2. 高速、低帧率、急转和停顿时，尾根连接、历史坐标保留，长度稳定；材质平铺不再裁断主体（任务4）。
3. 命中后服务器 Actor 结束，旧尾迹可自然收尾；取消、World 退出与池复用不会残留、复活或重复回收（任务6）。
4. Ribbon 的 RootName、ParamMapVariable、BoundVariable、类型与数据集一致；三个 ID 独立且冷加载不混线（任务1、4）。
5. 源图 Alpha、sRGB、线性数据通道、压缩/Mip 和曝光各自正确；审核图来自本轮资产，禁止旧帧或美化图充当实机证明（任务2、5、8）。

## 关键设计决定

| 项目 | 本轮决定 |
| --- | --- |
| 内核 | 完整 Sphere Mesh、Opaque/Unlit，直径70cm，均匀缩放；固定轮廓，表面流动，不用稀疏透明体积决定球形 |
| 外焰 | 同一System内100cm Mesh与120cm Sprite组合，金黄火舌沿球面向后卷曲 |
| 主尾 | 世界空间，最长400cm并按速度比例缩放；原始根宽240cm补偿源图留白，结合部饱满，向后收窄并透明，无可见齐切封口 |
| 金丝 | 一个 CPU Ribbon Emitter，三个独立 NiagaraID；每条8—12cm，起始偏移约(+20Y)、(-20Y)、(+12Z)，低频交错，避免规则线圈 |
| 柔光/火星/热扰动 | 柔光130cm、低 Alpha；火星10/s、寿命0.35s；局部低强度热扰动，Low关闭 |
| 速度 | 保留项目真实恒速默认3000、允许3000—5000cm/s；不照搬参考的1800，不增加加速度或改变碰撞半径 |
| 变速尾长 | 原生SpawnPerUnit按真实位移生成，历史点累计世界行进距离，最长400cm×User.TrailLengthScale，年龄上限0.5s；静止停止新发射，不以出生帧瞬时速度决定寿命 |
| 采样密度 | 主尾每约20cm一点；金丝每条约25cm一点。按实际速度算 SpawnRate，按画质缩放，避免只拉寿命堆粒子 |
| 色彩 | 黄白亮核→明黄/金黄包裹→橙金主尾；橙红用于尾端/稀薄外缘，避免暗红主体及整片纯白过曝 |
| 材质职责 | 五类：新Opaque Core、新Shell、新共享Trail、现有Glow、现有Heat；主尾、金丝和飞行火星使用同一Trail母材质的不同实例 |
| 命中收尾 | 现有平台实例进入有界 Stopping；局部主体停、历史尾迹衰减，超时0.5s兜底；World退出立即清理 |

参考方案的“7类/9实例”优化为七个实际 Emitter：Core、SurfaceFlames（Shell）、FireTrail、Filaments、Glow、Embers、AirRipple（Heat）。不同时保留七层和九层两套实现。内核、主尾是不可丢失信息；金丝、光晕、火星和热扰动可降级。

## 文件与资产职责

下列相对路径均从当前工作空间根解析：

- `Tools/Unreal/VFX/SpellProjectiles/PresentationManifest.json`：唯一当前映射、尺寸、材质角色、User参数、质量和退休资源合同。
- `Tools/Unreal/VFX/SpellProjectiles/AuthorFireballMaterialsUE58.py`：唯一纹理/材质作者；新增 `author_flight_textures(cfg: dict) -> dict`、`author_flight_materials(cfg: dict) -> dict`，旧入口调用共同实现。
- `Tools/Unreal/VFX/SpellProjectiles/BakeFireballDataTexturesUE58.py`（新增）：仅在本轮自有编辑器烘焙 CoreFlow、HeatRG、NoisePack 数值通道，不拥有 Niagara 或运行时状态。
- `Tools/Unreal/VFX/SpellProjectiles/IntegrateFireballNiagaraMonolith.py`：组合、完整 typed Ribbon 绑定及保存；移除自身重复的材质图/调色写入，调用唯一材质作者。
- `Tools/Unreal/VFX/SpellProjectiles/FireballMaterialParameters.hlsl`、`FireballSecondarySpawn.hlsl`：阶段参数和出生偏移，世界历史坐标不逐帧重置。
- `Tools/Unreal/VFX/SpellProjectiles/CaptureFireballFlightMotionUE58.py`：扩展同一采样器提供 `capture_review(view: str, quality: int, frame_count: int=48) -> dict`；每轮唯一目录、PID、资产SHA和相机，完成后才展示。
- `Tests/Assets/TestSpellPresentationContract.py`、`ValidateFireballNiagaraUE58.py`、`ValidateFireballFlightSimulationUE58.py`：合同、原生引用/Graph、真实粒子和质量回归。
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/`：任务6仅修改既有 Service、Executor、Registry、WorldSubsystem 与生命周期测试，不建立项目播放器。
- 内容均在现有 `.../Content/VFX/SpellProjectiles/{Textures/Fireball,Materials/Fireball,Niagara}/`；当前资源采用职责名，不加 V2/Review 后缀。

纹理优化为六项新增飞行源图，保留原稿RGBA与哈希；Glow复用现有程序化柔光，Ember复用已存在纹理，Impact复用现有翻页资源，不为清单数量重复制作。

| 当前源图/Texture名（无分辨率版本后缀） | 目标尺寸 | 制作/导入合同 |
| --- | --- | --- |
| T_DBA_Fireball_CoreFlow | 1024² | 原生数据烘焙，R熔融纹、G低频、B细线，sRGB=false、Repeat |
| T_DBA_Fireball_ShellRing | 1024² | imagegen参考绘制，真实透明、非相机固定圆环；彩色sRGB=true、Clamp |
| T_DBA_Fireball_TrailBroad | 2048×512 | imagegen，宽幅连续流纹，U首尾可平铺，无球头/圆封口，V边缘透明；U Repeat/V Clamp |
| T_DBA_Fireball_TrailFilament | 1024×256 | imagegen，一条细长流纹供三个Ribbon复用，避免三线纹理再乘三Ribbon；U Repeat/V Clamp |
| T_DBA_Fireball_HeatRG | 512² | 原生R/G低频向量场、A局部遮罩，sRGB=false、Clamp |
| T_DBA_Fireball_NoisePack | 512² | 原生RGB不同尺度无缝噪声，sRGB=false、Repeat |

## Task 1：固化合同与正确读取原生执行图

**Files:** 修改 PresentationManifest、Integrate、两个原生验证器及本计划；备份写入 `Saved/Validation/FireballFlight/<RunId>/Backups`。

**Interfaces:** `Author.verify_native(out_dir: Path) -> dict` 收集 Monolith `get_module_graph(..., links=true)` 实际 GetNodes 与 PID/资产SHA；`validate(report_path=None, raise_on_error=True, native_system_graphs=None) -> dict` 独立消费快照，不导入作者。

- [ ] 记录当前HEAD、脏文件、真实引用、三阶段包SHA及自有编辑器身份；不得覆盖其他改动。
- [ ] 保留RED：当前 UObjectIterator曾计44个对象，而原生实际Graph为14节点；禁止把不可达旧对象算作执行层。Ribbon绑定曾出现RootName/ParamMap/BoundVariable类型不一致，原始失败保留。
- [ ] 写单一当前合同：七Emitter/五材质职责、上表尺寸、六纹理、质量及退休Flight材质；所有读写入口读取同一合同。
- [ ] 原生验证实际14个Emitter执行节点、七层均连接；冷加载再查一次。静态运行 `python -B Tests/Assets/TestSpellPresentationContract.py`，预期全部通过。

## Task 2：纹理制作与第一组人工审核图

**Files:** 新增数据烘焙器；修改唯一材质作者、SourceArt/Fireball/SourceManifest.json；新增上表六PNG及对应真实Texture2D。

**Interfaces:** `bake_data_textures(cfg: dict) -> dict` 返回源图路径/尺寸/通道统计/SHA；`author_flight_textures` 返回导入路径及实际设置。

- [ ] 先做线性灰阶校准RED：0/0.5/1的数据PNG与导入回读应约0/128/255；发现重复Gamma不能靠调亮/调暗抵消。
- [ ] 原生烘焙三种数值纹理；分别调用内置imagegen制作三张绘制纹理。保留Alpha，最终文件复制回SourceArt；不使用未经授权的CLI/API替代。
- [ ] Monolith导入，核对RGBA、实际尺寸、sRGB、AddressX/Y、Alpha和合适压缩；平铺纹理检查U接缝与Mip远近闪烁。保留源PNG实际尺寸与原字节；建议GPU尺寸不冒充生成尺寸。材质平面预览按源图宽高比显示，Ribbon世界长宽独立受合同控制。
- [ ] **审核图A：** 每张源图RGB、Alpha/数据通道、透明棋盘底、原尺寸与游戏尺寸预览；明确源图与原生显示来源。用户指出偏差时在同一当前资源上修正。

## Task 3：先完成稳定圆球内核，再做五类材质

**Files:** 唯一材质作者、Native validator；新增 `M_DBA_Fireball_CoreSurface`、`M_DBA_Fireball_Shell`、`M_DBA_Fireball_Trail`；原生迁移 `MI_DBA_Fireball_Volume` 为职责名 `MI_DBA_Fireball_CoreSurface`，保留必要重定向追溯并从当前Cook闭包退休旧路径。新建Flight专用Shell/TrailBroad实例，金丝复用Trail母材质。

**Interfaces:** `author_flight_materials(cfg) -> dict` 返回五类母材质/实例映射及编译结果；Core唯一使用Engine Sphere、Opaque/Unlit、均匀0.7缩放，无改变轮廓的WPO。

- [ ] 原生RED断言：Core材质必须Opaque、可写深度、无透明孔洞；当前稀疏透明体积不满足。查真实引用后才迁移实例，未引用的通用母材质不扩大清理。
- [ ] CoreFlow双向低速流动，FireIntensity=4起步，柔和Fresnel/黄白到金黄，保留亮暗层次。外焰使用专用Shell母材质与实例（Translucent、sRGB Color采样）；既有灰度翻页母材质继续服务Cast/Impact，避免SamplerType与彩色纹理冲突；主尾/金丝共享新Trail母材质，RGB与Alpha只乘一次，U平铺不裁断。绘制纹理如有U接缝，使用两个相差半周期采样与sin²权重在材质中平滑拼接，各采样在自身断点权重为0；不靠重复Gamma或离线改色遮掩。
- [ ] Glow复用现有程序化渐变，Heat复用现有局部扰动母材质。整理Integrate与旧作者重叠写入为统一函数调用，旧入口不能重新生成旧Volume绑定。
- [ ] **审核图B：** 单独Core在0°/45°/90°及短转台视频；再展示Shell、Trail、Glow、Heat各自实际UE材质。必须先能清晰识别圆球，再叠加装饰。原生保存、材质编译、独立冷加载全部通过。

## Task 4：七Emitter、三条独立金丝与世界历史

**Files:** Integrate、两份HLSL、Flight System、只读粒子验证器。保持既有稳定System；Core/SurfaceFlames/Glow/AirRipple局部空间，FireTrail/Filaments/Embers世界空间。

**Interfaces:** `Author.bind_ribbon_id(emitter: str) -> None` 写入完整、同类型的NiagaraID绑定并回读；`validate(qualities=None, speed_cm_per_second=3000.0) -> dict` 检查真实SimCache。

- [ ] 记录当前ID缺失/混成线圈的RED；完整核对RootName、ParamMapVariable、DataSetName、BoundVariable、NiagaraID类型和实际源存在标志。不能只改显示名称或硬编码类型注册整数。
- [ ] 原生绑定三个稳定RibbonID，按组内出生顺序排序；偏移只在出生时写世界坐标，金丝低频交织。新模块插入既有适配前，保留有效更新链；重复作者不重复Emitter/模块，不使用已观察到会断链的末端裸删除方式。
- [x] 按世界距离预算更新尾迹，1800/3000/5000cm/s、60Hz位置阶跃、移动/转向、四画质、Drain与静止共10组RHI/SimCache回归通过；目标400cm（稳定后±10%），三个真实ID、旧点保留世界轨迹。30/60/120积分一致性另外由通用运动原生测试验证。
- [ ] **审核图C：** 侧面、斜侧、跟随视角固定帧与真实运动视频；三丝带临时隔离显示确认没有跨组连接，恢复组合后检查宽幅尾焰、根宽和渐细。

## Task 5：四档质量、曝光与审核证据

**Files:** 唯一合同、现有Effect Type/Renderer质量设置、Capture脚本、原生验证器。

**Interfaces:** `capture_review` 返回 `{run_id,pid,asset_hashes,camera,quality,status,images,video}`；状态Complete且资产SHA对应本轮后才向用户展示。

- [ ] 在现有Capture逻辑中统一相机/生命周期，移除天空干扰；每次捕获独立目录，不覆盖上一轮Frame文件。禁止展示仍Capturing的旧图、AI重绘截图或后期调色图冒充实机。
- [ ] Core与主尾各档保留；High三金丝，Medium可减到两条，Low关闭金丝/火星/热扰动并保留低强度柔光。Heat Alpha沿用0/0.5/1/1.5；每档实际生效后独立采样，结束恢复进程设置。
- [ ] 核对固定曝光及真实Village/MainArena自动曝光下亮核仍有形、金黄层不洗成白片、Alpha边缘无黑边/噪点。记录Shader Complexity/透明Overdraw及多人GPU样本，未测量不宣称预算达标。
- [ ] **审核图D：** 四档同镜头对照、明暗环境对照、贴近参考图的真实Native组合帧与动态视频；人工状态始终独立记录为待审核/通过/需修正。

## Task 6：命中自然收尾与既有平台生命周期

**Files:** `Public/Interfaces/GamePlatformVFXService.h`；新增 `Public/Types/GamePlatformVFXStopMode.h`；既有Execution/GamePlatformVFXNiagaraExecutor、Instances/GamePlatformVFXInstanceRegistry/Record、Subsystems/GamePlatformVFXWorldSubsystem；`Private/Tests/GamePlatformVFXLifecycleTests.cpp`；项目已存在Projectile事实桥的Stop调用点。

**Interfaces:** 保持 `Stop(Handle)` 兼容；增加 `StopWithMode(Handle, EGamePlatformVFXStopMode)`，两入口调用同一停止实现。Immediate立即清理；Drain进入既有Stopping，由OnSystemFinished或有界超时收敛。

- [ ] 添加原生RED三项：`GamePlatform.VFX.ProjectileDrain.SurvivesSourceDestroy`、`WorldExitCancelsDrain`、`StaleHandleCannotStopReusedComponent`。
- [ ] 确认命中才用Drain；沿现有Executor为需要尾迹的组件提供World所有权，解除附着后服务器Actor可正常结束。局部主体Emitter在Inactive时Kill，世界尾迹/火星Complete，最长0.5秒兜底；不延迟服务器碰撞、结算或销毁，不新增本地VFX Actor。
- [ ] Stopping保留原句柄/代次/World与池记录直到真实结束；取消、世界退出、租约撤销走Immediate，清理超时句柄；不能用IsActive=false提前回收仍有尾粒子的组件。
- [ ] 三项GREEN，并拍摄命中、取消、销毁、换世界动态证据。VFX失败不抑制SFX，沿原命中去重合同确认一次音效和一次表现。

## Task 7：独立审查、完整回归及同步双端打包

**Files:** 完成当前Pack Docs/FireballMaterialKit.md、MonolithGenerationManifest.json、当前合同、Native验证；使用既有Build/Cook入口，证据输出新RunId。

- [ ] 定稿原生资源和插件文档，再做一次独立只读全改动审查；修复重要问题。本轮不自动提交其他未提交修改。
- [ ] 固定参数完整Editor增量构建；运行新增Drain三项、全部19项Projectile原生回归。已有<30度冻结直追分支继续使用：29.9/30/30.1、手部偏移、移动目标与真实Sweep均须GREEN；不重复重写权威算法。
- [ ] 固定参数完整Client/Server增量构建；Client执行 `CookFrontEndClient.ps1 -Cook -CustomConfig VillageDevelopmentClient -SkipZenStore -ForceNiagaraCompileOnLoad -EditorBuildResult <本次结果>`；Server执行 `CookFoundation.ps1 -Cook -Target Server -CustomConfig VillageDevelopmentServer -Pak -SkipDebugSymbols -SkipZenStore`，地图覆盖Village/MainArena。
- [ ] 核对同一次Cook的真实Niagara CPU/GPU编译、当前绑定、退休资源拒绝、必要资源闭包及Server表现剥离。失败修真实问题，禁止降低门禁。恢复获准临时关闭的柳树编辑器原地图和镜头。

## Task 8：两客户端按1实机审核与交付

**Files:** 复用本轮Saved下服务器受控替换、真实版本门禁、客户端启动器；最终报告和媒体写入同一RunId。

- [ ] 受控替换实际Village/MainArena测试实例，核对构建输入、Cook、EXE SHA、PID、BootId、端口和复制日志；保留账号资料。通过后运行两个不限帧客户端。
- [ ] 使用获准Computer Use在两个真实窗口分别按1施放；记录前摇、圆球头、宽尾、交织金丝、命中自然收尾及声音时机。真实进入Village和MainArena分别留证；门户故障未修复或准入未完成，不能写联机通过。
- [ ] **审核图E：** 双客户端固定截图、真实施法/命中视频与必要系统声音；主观听感和艺术认可由用户确认，日志和进程存在不代替人工签收。
- [ ] 发布报告区分全部门禁；恢复相关任务协作。人工未通过、GPU未测量时明确待完成，不标Production Ready。

## 当前基线与执行状态

**最新状态以本段和Saved/Validation/ProjectileMotion20261101/Progress.md为准，下方旧PID及23项等为历史执行记录。** 通用运动已取得完整Editor与36项原生GREEN；后续绑定重入跟踪强化待最终构建。七个Profile、平滑Ease-Out CurveFloat和L_DBA_ProjectileMotion_Demo已Monolith保存。火球1200→1800cm/s、0.15s，持续追踪原目标。当前24项纹理/材质，70cm完整球头，100cm Mesh+120cm Sprite外焰，400cm世界尾迹、原始根宽240cm，尾根前15%饱满、球内5%淡入、尾端14%羽化透明。三阶段0错0警告、10组当前SHA动态回归通过；双端Cook/网络/声音/GPU/人工签收仍须分别留证。

- 计划改写时为main/d8e0eacd，工作树有本轮与既有修改；本任务未执行Git写操作，不回滚期间变化。
- 现有七层样例曾编译0错0警告，但头部艺术不通过、Ribbon完整绑定错误仍待修；已有旧图和旧通过记录仅作过程证据。
- `<30°仍追踪只取消弧线`源码已有，3项旧行为RED保留；最新完整19项GREEN尚未执行。
- 旧运行客户端/服务器不是本优化方案完成包。本计划不把当前进程、旧Cook或临时Native样例写成新方案验收。
- 当前自有编辑器为41480/9320；31584已按明确授权关闭，原镜头在Saved/Validation/FireballTrail20261101/SharedEditor.BeforeClose.json，任务7须恢复。

## 自审结果

已覆盖源方案的纹理/导入、五类材质、七层组合、世界尾迹、变速、命中收尾、质量/GPU、原生保存、双端构建与人工图像审核。已移除方案中第二工程树、第二Projectile BP、1800cm/s权威速度和九实例的照搬；六纹理/七Emitter是当前明确选定实现。后续任一参数/引用变化同步更新唯一合同与证据，不创建另一份当前实现。

### 纹理阶段人工反馈
用户已明确回复“三张源纹理合适，保持此方向”。三张源图原字节已保留：TrailBroad/TrailFilament为2172×724 RGBA，ShellRing为1254×1254 RGBA；GPU导入尺寸仍需原生核验。此反馈只覆盖源纹理，不代替材质与实机签收。

### 用户补充动态与照明合同
核心形状稳定，内部缓慢旋转流动；外焰沿球面向后卷曲；主尾实时延展并衰减，金丝沿轴线低频S形交织，火星少量脱离迅速淡出。Glow改为单粒子CPU Emitter，在同一Emitter增加Niagara Light Renderer（总Emitter仍7），High/Epic默认最多一盏局部金黄色粒子动态光、初始半径200cm，Medium/Low关闭真实动态光但保留材质自发光。光色/亮度与Sprite分别绑定，停/取消/World退出同寿命清理；不增加新Actor/System。任务5提供灰色地面及角色的光开/关A/B原生图与GPU样本，证明真实照亮环境；引擎粒子灯光不支持的软阴影能力不虚构为已实现。

球面轮廓检查发现通用Sphere仅960三角面，近景可见折角；使用唯一SM_DBA_Fireball_CoreSphere（64×32、3968三角面、100cm源直径），由合法OBJ源经Monolith原生导入，避免重建复制网格的RenderData资源断言。权威碰撞不受此视觉网格修改。

### 执行裁定与当前证据（2026-11-01）
继续授权后自有编辑器42384/9320已恢复；启动模态阻塞41480未执行资产修改。球体原生OBJ导入成功，3968三角面及100cm范围已读回。23项材质/纹理保存后验证通过，真实Graph.GetNodes三阶段为8/14/8节点、0错0警告。

Ruling: 原始Ribbon宽210cm补偿彩色源图透明留白，可见根宽由原生帧核验；不是将权威弹体放大。UV改全幅1×1，新端在球体内部淡化，取消旧灰度图裁切参数。
Ruling: 旧固定SpawnRate验证不再用于当前按距离出生；使用真实Slate帧移动、原生SpawnPerUnit和SimCache回读。3000cm/s当前样本约440cm尾迹、3个NiagaraID、单70cm核心和单200cm光源；图像认可、冷重载和GPU仍独立。
Ruling: 所有权RED使用真实原生SpawnSystemAttached复现来源销毁使组件失效，保留SourceOwnership.RED.json；新增3项C++生命周期回归在完整目标构建后运行，不伪称此前已执行。确认命中字段仅允许表现自然收尾，取消/World退出保持Immediate。
Ruling: 当前交付目标是单技能及两客户端人工验证；不将此范围扩展成所有技能/全10人生产预算认证。多人GPU发布门禁未完成时明确未认证，不用候选预算作承诺。
