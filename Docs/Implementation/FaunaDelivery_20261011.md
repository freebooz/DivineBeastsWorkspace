# 飞鸟审查与蝴蝶、鱼群交付说明

日期：2026-10-11  
工作区：E:/work/2026/DivineBeastsWorkspace  
引擎：D:/UnrealEngine-5.8.0-release，UE 5.8.0  
本轮范围：现有飞鸟审查和修复、通用群落机制、两种真实资源、项目接入及运行验证。

## 1. 已实现的结果

现有飞鸟的五项项目编排缺陷已修复。蝴蝶与鱼群已经接入SSLM现有的项目物种目录、ContentPack、资源租约、表现请求与平台 VFX 世界实例。

实际交付包含蓝紫蝴蝶 ButterflyAzure、银蓝鱼 FishSilver 各一套可编辑源模型、FBX、纹理、PBR 材质、三档静态网格 LOD、Niagara System、WorldDefinition 和场景 Habitat 蓝图，以及可直接运行的审核关卡。所有 uasset/umap 均由真实 Unreal Editor 创建、导入、编译和保存。

当前方案采用固定步长 CPU 群落模拟，向 Niagara 批量上传位置、朝向和动作状态，由 GPU 网格渲染。它没有实现 GPU Boids；运行路径没有为每个个体创建 Actor，也没有依赖 GPU 回读推进游戏逻辑。测试中的 GPU 回读只用于验证。

## 2. 飞鸟审查及修复

详细证据和函数定位见同目录 BirdFlockAudit_20261011.md。确认的五项问题如下。

### 2.1 默认标识冲突及误停止

旧默认身份不能正确区分同一 Actor 上的多个场景组件。现在空 ID 使用完整组件路径；显式 ID 同时校验 Owner 与 RegistrationToken。一个组件失败或退出时，只能撤销自己的那代注册。

### 2.2 非法替换破坏旧的合法绑定

现在先验证候选并受理新绑定，再撤销旧绑定。物种、数量、种子等候选不合法或新 ID 已被占用时，旧有效请求继续存在。组件自身绑定也按事务顺序发布。

### 2.3 取消通知同步重入覆盖较新意图

取消不是保证异步返回的接口。现在操作先发布或撤销当前意图，再调用可能广播的接口；返回后使用 MutationToken 重新核对。没有把跨回调保存的 TMap 元素引用继续当成有效当前状态。

### 2.4 同一世界切换 Context 丢失自定义群落

同一 UWorld 的 Context 挂起与恢复保留当前期望配置；挂起期间的更新或停止按最新意图生效。世界退出、账户退出等真正的生命周期终点清理相应状态。

### 2.5 实际播放终态未投影到项目 RequestId

现在订阅平台实际完成通知，并以世界、绑定代次、顶层 RequestId 等条件精确匹配。终态清除当前播放身份，保留可显式重试的期望配置，避免相同参数无法恢复或隐式无限重试。

这些修复复用于蝴蝶与鱼群的共用环境群落编排。原有飞鸟仍使用现有美术和运动方案，本轮没有把飞鸟改造成新的 Boids 算法。

另外修复了平台原生 Niagara 池的激活前失败清理：未激活 AutoRelease 组件不能仅依赖 DeactivateImmediate 自动归池。生产 Registry 现在按实际组件池状态回收或销毁，并保护同步回调中的新 Owner，避免旧清理操作回收已经复用的组件。

## 3. 三层逻辑架构与独立复用边界

### GamePlatform 平台层

GamePlatformVFXClient 提供不含项目物种名和项目资源路径的行为算法、通用 Fauna 配置、世界实例绑定、数组上传、受惊入口和只读诊断。它附属于现有唯一 VFX 世界执行链，使用既有 Niagara 原生组件池及 EffectType。

GamePlatformData 继续负责逻辑资源身份、加载及租约；GamePlatformPresentation 继续解析中立表现请求。没有增加第二资源加载器、第二 VFX 播放器、第二预算管理器或公开私有 WorldSubsystem。

通用平台资产位于 /GamePlatformVFX/Environment/Fauna，默认资源不反向引用SSLM物种包。

### MobaCommon 玩法公共层

这次纯环境表现不需要新增 MOBA 业务能力，因此未为调用链强行增加一层转发。如果后续战斗、技能或阵营规则要触发惊扰，应由玩法层产生中立表现意图；伤害、捕捉、奖励和权威判定仍属于玩法逻辑。

### DivineBeasts 项目层

物种语义、GameplayTag、LogicalId、模型和材质归项目。DivineBeastsPresentationClient 处理场景生命周期及项目请求编排；DBAPresentationPack_Core 提供可替换的资源内容。

这种边界允许其他项目复用 GamePlatformVFX 中的机制与平台模板，再提供自己的目录映射和物种资产。独立复用指现有平台插件的明确依赖及内容边界，并非要求新增一个项目专属的空插件。

## 4. 实际行为与限制

### 蝴蝶

实现游飞、择花接近、停栖、起飞、受惊五态。花点有位置、归一化表面法线及容量；接近时预约槽位，离开、超时和受惊释放。没有花点或无空槽时继续游飞。

PerchOffsetCm 默认 0.72 cm，用于本轮模型脚底接触。它与 BodyRadius 分开：身体约束半径仍用于盒边界、球障碍和槽位合法性，不通过缩小碰撞半径掩盖脚底悬空。

### 鱼群

实现有界邻居查询、分离、速度对齐、局部聚合、巡游和受惊。空间网格和旧状态快照避免顺序更新偏置，并限制邻居数量及候选检查量。

鱼的中心保持在扣除 BodyRadius 的盒体、简单 XY 水域多边形和球障碍约束内。移动做连续线段检查，防止合法终点掩盖途中穿过岸线或岛屿。水域高度范围由 HalfExtent.Z 定义；当前不是对 Water 插件动态波面或任意三维导航体的实时采样。

### 更新与表现

行为采用 30 Hz 固定步长，最多补算八步。显示端插值位置、朝向及动作参数；正常情况下有一固定步的插值延迟。Niagara 原生 PauseResume 是当前 EffectType 的裁剪响应。

数组使用稳定的 Particles.UniqueID 与一次 Burst 对应。实际 GPU 验证曾抓到错误外部索引导致所有粒子重叠的问题，现已修复并通过正式链路的独立位置检查。

每群当前数量上限为 512，这只是输入容量合同，不是任意平台和任意同屏群落数的性能保证。网格有三个 LOD，Niagara 使用 PerParticle 模式按个体屏幕尺寸选择，避免生成 LOD 后仍固定使用第 0 档。

## 5. 直接在SSLM中使用

### 打开审核关卡

打开 Game/DivineBeastsArena.uproject，在 Content Browser 输入：

/Game/Development/Fauna/Maps/L_DBA_FaunaReview

运行 PIE。关卡内的正式 Habitat 蓝图自动创建 32 只蝴蝶和 64 条鱼，具备花面、水域、球障碍及三台审核摄像机。

摄像机标签为 FaunaReview_Camera_Overview、FaunaReview_Camera_Butterfly、FaunaReview_Camera_Fish。默认自动激活总览机位；近景可主动切换。

### 在目标场景放置群落

使用下列真实蓝图：

- /DBAPresentationPack_Core/VFX/Environment/Fauna/Blueprints/BP_DBA_ButterflyAzureHabitat
- /DBAPresentationPack_Core/VFX/Environment/Fauna/Blueprints/BP_DBA_FishSilverHabitat

选择组件后配置：

- HabitatId：通常保持空值，自动按完整组件路径区分实例。需要稳定业务身份时再显式设置，不能让两个活跃 Owner 占用同一个值。
- SpeciesId：ButterflyAzure 或 FishSilver。
- Count：0 表示停止，1..512 表示请求数量；默认蝶 32、鱼 64。
- Seed：0..1000000；默认蝶 137、鱼 409。
- 位置：作为群落的世界原点。

BeginPlay 自动 ApplyFauna，EndPlay 自动 StopFauna。运行时更改数量、种子或移动锚点后，显式调用 ApplyFauna。返回 true 仅代表受理请求，不能用来代替 GPU 可见性验证。

### 调整花点、水域和障碍

配置位于两份 WorldDefinition 的 Fauna 字段：

- /DBAPresentationPack_Core/VFX/Environment/Fauna/Definitions/DA_DBA_VFX_Fauna_ButterflyAzure
- /DBAPresentationPack_Core/VFX/Environment/Fauna/Definitions/DA_DBA_VFX_Fauna_FishSilver

Flowers、WaterPolygon、Obstacles、HalfExtent 使用相对群落原点的局部厘米坐标。花点应与实际可见花面相匹配；鱼的盒体和多边形应根据实际水体落位。

当前场景接口只传递世界位置，没有传递 Actor 的旋转和缩放，也没有逐 Habitat 的布局覆盖。因此，同物种的多个 Habitat 拥有独立状态，但共享同一布局模板并分别平移。复制 Blueprint 不会自动复制并改用一份独立 Definition。

调整 Definition 后，执行 StopFauna → ApplyFauna 或重新进入 PIE，使新实例复制最新配置。仅在输入完全相同时再次 Apply 会保留当前请求。新增物种或新的布局类型需要在项目目录登记独立语义和 LogicalId，不能只复制资产而期待现有 SpeciesId 自动发现它。

### 惊扰与诊断

DisturbFauna 使用世界中心、半径、强度和持续时间。入口范围内符合条件的当前世界群落可以同时受影响；它不是仅作用于调用组件的单群操作。当前生产入口强度 0..1，持续时间不超过 60 秒。

QueryFaunaState 返回生产 CPU 的数量、停栖数、预约槽位数、渲染活动标记和局部位置；它是诊断快照，不是 GPU 回读。CPU 显示插值意味着同一帧 CPU 当前状态与 GPU 插值位置可不同。

当前这些个体是客户端环境表现，不承载捕捉、碰撞伤害、资源掉落、任务结算等权威玩法，也不逐个复制网络状态。

## 6. 资源与制作入口

平台运行资产：

- Niagara/NS_GP_Fauna_Base
- Niagara/NM_GP_FaunaArrayMotion
- Materials/M_GP_Fauna_Master
- Materials/T_GP_Fauna_NeutralRoughness
- EffectTypes/NET_GP_Fauna_Ambient

项目物种资产使用 SM_DBA_、MI_DBA_、NS_DBA_、DA_DBA_VFX_Fauna_ 前缀；贴图包含 BaseColor、Roughness、Normal。可编辑源资产位于 DBAPresentationPack_Core/SourceArt/Fauna。

可重建工具位于 Tools/Blender/Fauna 和 Tools/Unreal/VFX/Fauna。制作链分别承担模型源导出、UE 原生导入/材质、Niagara 图及物种配置、Definition/Blueprint/Map、网格 LOD。不能用文本或假文件替代 uasset。

正式逻辑定义为：

- presentation.dba.environment.fauna.butterflyazure@1
- presentation.dba.environment.fauna.fishsilver@1

DefaultGame 中的既有 AssetManager 扫描条目已加入 Fauna Definitions。FrontEndClient 与 VillageDevelopmentClient 已登记两条运行资源根。DedicatedServer、VillageServer、VillageDevelopmentServer 已登记两条 Fauna 根及开发审核目录的 NeverCook；其中后两项使实际 Cooker 配置也直接拥有排除规则。

## 7. 本轮已完成的运行证据

证据根均位于仓库根 Saved/Validation，不属于发行资源。只列本轮实际读取的结果，不把启动脚本返回 0 当作异步检查已经通过。

### 原生生产算法：17 passed / 0 failed

Saved/Validation/FaunaNative/Verified_PerchOffset_20261011/Result.json、Compile.log、Tests.log。

使用实际 MSVC 14.44.35207，C++17、/W4 /WX /O2，直接编译生产算法。包括数量和非法输入、种子重现、花点容量、停栖/起飞/受惊、鱼群邻居作用、凹水域和连续障碍约束、微小几何及脚底接触偏移。长稳用例是十分钟模拟时间，不是十分钟实时 GPU 性能测试。

### 真实组件所有权：9 + 13 项通过

- Saved/Validation/BirdFlockAudit/OwnershipReview.json：飞鸟 9 项。
- Saved/Validation/BirdFlockAudit/FaunaOwnershipReview.json：Fauna 13 项。

均在真实 PIE 世界和真实 LocalPlayer 服务中创建实际组件，检查默认身份、Owner 冲突、非法替换、边界参数、Stop 和 EndPlay。使用运行态赋值后显式 Apply，避免将编辑器 Pre/PostEdit 的组件重建事务混入测试。它们不单独证明 GPU 数量。

### 正式场景到 GPU：10 组通过

Saved/Validation/Fauna/IntegrationReview.json，status=passed。

两物种分别验证 0、1、32、128、512。非零组实际读取 GPU Position、MeshOrientation、DynamicMaterialParameter，检查粒子数、有限值、单位四元数、动作范围，以及独立计算的盒体/岸线/障碍约束。

同时实际观察到：场景 Blueprint 自动启动；32 只蝶出现停栖并占用花槽；惊扰后停栖数归零；128 条鱼的 GPU 位置持续变化；真实销毁 Niagara 组件后没有隐式自动重试，相同参数显式 Apply 恢复 128 个 GPU 个体；StopAll 后活跃 Fauna Niagara 数为 0，结束 PIE 后游戏世界清理。

### 资源验证与 LOD

Saved/Validation/Fauna/Integration/FaunaIntegrationAuthoring.json：2 Definition、2 Blueprint 和审核地图实际制作及登记验证。

Saved/Validation/Fauna/MeshLODs.json：蝶各 LOD 顶点数 2721 / 1641 / 596，鱼为 3176 / 1739 / 626，Niagara PerParticle 模式已回读。

Saved/Validation/Fauna/MeshLODNiagaraCompile.json：两个物种系统在 LOD 配置后重新编译，均 0 错误、0 警告。

作者阶段报告中的 runtime/manual pending 是当时的历史快照；后续运行结果应读各自的实际运行报告。

### 飞鸟三物种新一轮图形回归：15 组通过

Saved/Validation/BirdFlockAudit/FaunaRegression_20261011T150801/IntegrationReview.json，status=passed。海鸥、燕子、鹰各验证 0、1、32、128、512，15/15 GPU 数量正确；自动场景通过，StopAll 后活动 Niagara 为 0，worldCleanup=true，实际测试阶段 34.516 秒。

Inputs.json 记录本轮 harness 以及平台/项目两个实际 DLL 的 SHA-256，运行前后均未变化。VerifiedSummary.json 严格核对矩阵、清理及输入指纹。报告 SHA-256：BED47E58B19E2448906D8C9720CFFB5F1E3A5380ABDB0B177C2280EF6B116F90。

### 暂停与实际视口：四项通过、三张截图

Saved/Validation/Fauna/VisualReviewLatest.json 为 captured。通过独立重新启动的真实图形编辑器加载已保存的审核地图和资源，验证原生 Niagara 暂停/恢复、游戏世界暂停/恢复共四项：暂停时 CPU 位置冻结，恢复后鱼群继续运动。实际检查阶段 15.484 秒，编辑器自动正常退出，进程 exit 0。

截图均为实际 PIE 游戏视口 HighResShot，1600×900，位于 Saved/Validation/Fauna/VisualReview_20261011T150552/：

- Overview.png：769ade6eba9ef1f46eda91e5de1eea36166f1754062676bed9e30ea9c7af8c13。
- Butterfly.png：eff803701b249a294e5b00e576c3ed47ae7602d53b2b27bda24e2b27d273a03d。
- Fish.png：b9d5948ec45a4f010862426011ee64fe9ee821ac0429625adeafd94ee206daa3。

两次独立技术视觉检查都能看到真实蝶翼、收翼停栖和银蓝鱼体/尾鳍，未见明显网格爆裂、严重变形或材质丢失。静态图片不能单独证明整个运动过程都避障，也不能精确验证亚厘米足点；这些结论应分别引用动态 GPU 检查和原生算法测试。

审核图使用简化花床、透明水池和球形障碍，并保留开发用 Preview 阴影。鱼群个别高光较亮、停栖蝶翼内侧偏暗，仍可在正式场景近景中调整。技术可见性检查不替代用户的最终美术认可；原始自动报告 imageInspection/humanArtAcceptance 的 pending 历史字段不改写为人工认可。

## 8. 实际构建与原生池生命周期验证

本轮确实运行过 UHT/C++ 编译和 Editor 模块链接，并在加载新接口的真实编辑器完成上述完整链路测试。第一次定向构建的最终失败来自引擎 Niagara/NiagaraEditor/Landscape 导入库缺失，另一次链接遇到并行 Editor 占用 DLL；两者均保留失败记录，没有修改引擎接口或伪造库。

后续成功链接记录包括：

- Saved/Validation/FoundationM0/70f94cfe-5846-4dfc-935d-27a80bf14b90/Build/Editor/UBT.log
- Saved/Validation/FoundationM0/198bfe0a-7265-496d-a472-990e749ba266/Build/Editor/UBT.log
- Saved/Validation/FoundationM0/f5063ac6-bd72-4d56-bd87-9411700e9bd7/Build/result.json：最后一次 GamePlatformVFXClient 定向 Editor 构建，实际编译/链接成功，退出 0。OnlyModules 非空、UsePrecompiled=true，不能称为完整 Editor 或引擎源码重建。

### 最终原生池回归：1 passed / 0 failed / 0 warnings

Saved/Validation/FoundationM0/edc876ad-bb96-46da-a6c9-1db90b604758/Test-FaunaPool/result.json 与 Report/index.json，实际测试为 GamePlatform.VFX.Fauna.NativePoolPreActivationLifecycle。

该用例使用真实平台 Niagara System、真实 EditorPreview World、Niagara NativePool 和生产 Registry，验证未激活 AutoRelease 的 FreeInPool 归还、同一真实对象复用、旧句柄隔离、重复 Stop 不产生重复空闲条目，以及关闭池或 None 模式时实际销毁组件。它不计入前面的 17 项纯算法测试。

最终夹具使用引擎 FScopedCVar 的两个独立 Tag 作用域恢复值与原始优先级，Registry/World 先清理、外层临时值最后撤销。真实 RTX 4090 / D3D12_SM6 进程 PID 44100 在独立 SessionId 中运行一项用例，进程正常退出 0；自动化报告只有一个对应设备，TestLoops=1，旧 SetWithCurrentPriority 诊断为 0。进程总耗时 26.909 秒，用例实际 0.257 秒。

Inputs.json 保存并确认测试前后四项输入 SHA-256 未变化：测试源、生产 Registry、基础 uasset 和真实加载的模块 DLL。最终测试源 SHA-256 为 0162949F3ABB8F95EBD4C80D085C1F1117B7A397D3BCB04485CAA12BB8A67BFF；模块 DLL 为 B69C4C4BA818D1A1BE004579E10C604966BF8F6FA57DEC39BAFDE535F0F71A07；Report/index.json 为 07C0183F4B6F11773077062B94B9245144D92C4675EF55ECA665386EBE3415FE。

之前 a2949118-b6a9-4f19-b5c4-a3bde382370b 的旧夹具也实际通过，但仍有三条 ConsoleManager Display 诊断；保留历史报告，最终结论引用上述新运行。

### 完整 Client / Server 构建证据

完整 Client 最新记录为 Saved/Validation/FoundationM0/d706fa7b-0d26-47f3-98de-0d6475b75194/Build/result.json：DivineBeastsArenaClient，Win64 Development，OnlyModules 为空、UsePrecompiled=false，Status=Passed、ExitCode=0，实际 52.098 秒。对应 UBT.log 显示 Result: Succeeded。

完整 Server 的实际 UBT 日志 Saved/Validation/FoundationM0/2dc35ef2-850a-4020-8b2c-0314f720fcc2/Build/Server/UBT.log，命令目标为 DivineBeastsArenaServer Win64 Development，未传 Module 或 UsePrecompiled；最终 Result: Succeeded，3221.90 秒。该运行没有可读的总 Build/result.json，因此这里严格引用底层 UBT 目标成功，不声称外层脚本所有后处理已完成。

这些完整目标由共享工作区中的并行构建执行，本任务实际回读了日志及最新 Client 结构化结果。它们是各自构建时的证据；后续最后修改仅是 WITH_EDITOR 包围的原生池测试夹具，没有改变生产算法或物种资产。

## 9. Cook 与发行验证范围

已独立回读 WindowsClient 定向 Fauna 资源 Cook 和 VillageServer 正式地图 Cook 的真实完成日志：两者均 result 0、0 error、0 warning。分别位于 Saved/Validation/Fauna/AssetCook_20261011T2256/Cook.log 与 Saved/Validation/Fauna/VillageServerAssetCook_20261011/Cook.log。客户端实际 Cooked 568 / Skipped 8；服务器 Cooked 644 / Skipped 9。

项目使用 Zen cooked output store；不能把普通目录下没有松散 uasset 当成资源遗漏，也不能只靠二进制字符串命中证明包级收录。

官方 DumpAssetRegistry 已实际解析两份 Cook AssetRegistry.bin，报告为 Saved/Validation/Fauna/RegistryDump_20261011T152830Z/CookRegistryAudit.json，status=passed。Client 完整包数 568、资产对象 574，20/20 必需完整包名匹配，missingPackages=[]、forbiddenPackages=[]；Server 完整包数 644、资产对象 663，两个 Fauna 运行根及开发/ExternalActors/ExternalObjects 范围均为 0，正式 Village 地图存在。分页 Begin/End、声明包数、对象数和输入 SHA 均校验通过，报告 SHA-256 为 9b25bc501f14ca3dc5a6efb9d50f5e4f25e862c7ad7de4780eda07f34ea10488。

两次 Dump 进程均实际退出 0，各有 0 error、1 条既有 GamePlatformSpawnEligibility Python 暴露重名 warning；不能把这两份 Dump 日志记为零警告。该报告证明本次 Cook Registry 包名收录/排除，不证明可变 Zen oplog 的当前 payload，也不覆盖原始 FBX/PNG 等松散文件。

### 正式 FrontEndClient Cook/Stage 与主资源容器

既有正式入口实际运行 f0e7c867-1d0f-4d56-98a8-8462b2c20fac，Saved/Validation/FoundationM0/<RunId>/Cook-FrontEndClient/result.json 为 Passed、ExitCode=0。该运行保留真实 UI RHI、前端和鸟群门禁，生成独立 Stage 下的 Client.exe 与 IoStore；其实际 UAT 阶段 33.591 秒。

本次真实 UnrealPak 导出的 ContainerInspection/ClientContainer.csv 有 2761 行数据、1320 个唯一文件。使用项目原有 Tests/VFX/Fauna/ValidateFaunaContainer.py 执行后，20 个必需资源全齐，missing=[]、forbidden=[]，Fauna 文件共 26 个（包含运行包的附加数据）。报告位于 Saved/Validation/Fauna/FormalClientStage_f0e7c867/FaunaContainer.json。

共享机器的执行槽被其他常驻任务占用，因此取消了两个尚未启动的只读排队任务，改为读取完整 CSV 和同一审计器，在本地 Python 3.12 离线执行。重建副本后先校验原始字节 SHA-256：CSV=60509026ee1cb4db34eb0ef2270ceb839de826c9262710a7882d7f73fbcc37c8（885900 字节），审计器=05a2d79c6af73e9c1f0d6af8adf8ce8908a31390a3cec22bcb5b1e4b87c6745f（7922 字节），均与项目端完全一致。ExecutionReceipt.json 记录真实离线执行环境、退出 0 及原始远端路径；没有把这次 Python 检查称为 Windows 原生运行。

StageInventory.json 来自项目目录的完整只读分页，列出主 .utoc/.ucas/.pak 与 global.utoc/global.ucas。本次 Fauna 清单结论严格覆盖 DivineBeastsArena-WindowsClient 主资源容器；global 和 .pak 内部内容未另行展开为 Fauna CSV。它也不代替打包客户端内的蝴蝶/鱼群运行、美术验收或最终 Server Stage 检查。

FaunaCookValidationPlan_20261011.md 保留既有正式入口和范围；实际结果同时记录于 FaunaContainerAuditValidation_20261011.md。服务器最终 Stage、目标硬件同屏帧预算和正式新手村场景布置仍需对应的实际验证。

实际视口和暂停证据见上节。此前截图测试失败来自 UE 5.8 未暴露 PlayerController.console_command，已修正为实际 SystemLibrary.execute_console_command 入口并重新运行成功；失败的历史运行目录仍保留。
