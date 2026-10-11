# 神兽联盟正式飞鸟审查与环境群落接入记录

日期：2026-10-11  
审查基线：`bd106b3259c1a1917c0ece46d3ec2a8edd227fd4`  
工作区：`E:/work/2026/DivineBeastsWorkspace`  
引擎：`D:/UnrealEngine-5.8.0-release`

## 1. 结论与证据边界

本轮确认五个项目编排缺陷：取消通知重入会覆盖较新的意图；组件默认身份碰撞及按 ID 误停；换 ID 的非法候选会先破坏合法旧绑定；同一 UWorld 的 Context 变化会丢弃自定义场景意图；实际异步播放终态未反馈到项目 RequestId，导致相同参数无法重试。已在项目层实现相应修正，并将修正后的同一套事务用于蝴蝶与鱼群。

这是源码审查、实现及最终验收证据补充记录。子任务本人没有启动/关闭编辑器、构建 C++、执行 PIE、运行图形验证或提交 Git；现已直接回读主任务生成的最终真实 PIE 报告：鸟群 Owner 9/9、Fauna Owner 13/13、Fauna 两物种 10 个 GPU 数量用例及终态显式恢复通过；飞鸟既有三物种 15 组 GPU 矩阵也已在本轮真实复跑并全部通过。具体归档、输入指纹、覆盖范围和仍待验证的交错见第 6 节，不沿用历史“通过”作为本轮 GPU 证据。

正文中“原始行”指上述 Git 基线；“当前行”指本轮项目编排交回时的源码快照，后续合并可按函数名定位。

## 2. 正式链路与所有权

正式场景组件只把物种、数量和世界锚点转为项目中立请求。项目目录用 `GetBirdFlockTag/BuildBirdFlockFragment` 或 `GetFaunaTag/BuildFaunaFragment` 选择逻辑 Definition ID；既有 ContentPack 事务通过 GamePlatformData 申请真实租约，全部必需定义就绪后才注册目录。平台 Presentation 解析中立请求，唯一 GamePlatformVFX 世界执行器拥有 Niagara 实例和播放预算。

本轮没有增加 Niagara 播放器、逐鸟/逐鱼 Actor、项目资源加载器或新 VFX 插件。新组件只在 BeginPlay、显式 Apply、编辑器 PIE 属性变更和 EndPlay 事件中工作，没有自身 Tick。

每个当前意图包含：

- **ID**：含义：Bird 的 FlockId 或 Fauna 的 HabitatId；两个类型的目录分别命名，不相互占位。
- **Owner + RegistrationToken**：含义：场景拥有者与注册代次；空 token 条目属于显式服务请求。
- **MutationToken**：含义：每次配置变更的意图代次；外层操作在同步广播后必须仍匹配才能继续。
- **RequestId**：含义：当前提交到平台的中立请求身份；挂起/停止在广播取消前先撤销，真实顶层终态精确匹配后清除。
- **bAwaitingExplicitRetry**：含义：终态后保留意图但暂停隐式再提交；该 ID 显式 Apply 或新 Context 恢复重新准备一次。
- **World**：含义：该意图绑定的真实 UWorld；过期世界的意图不迁移到后来世界。
- **Species/Count/Seed/Origin**：含义：配置值；Count 是请求数量，不是可见粒子计数。

## 3. 已确认的问题

### BIRD-01：取消回调重入可覆盖最新意图或复活已经停止的群落（P1）

原始位置：

- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Environment/DivineBeastsBirdFlockPresentation.cpp:47–54`。
- 原实现先复制旧状态并使 RequestId 失效，再调用 `CancelBirdFlockRequest`；返回后无条件 `AmbientBirdFlocks.Add(FlockId, Desired)`。

平台停止最终通过 `UGamePlatformVFXWorldSubsystem::CompletePlayback` 同步广播 `FGamePlatformVFXPlaybackCompleted`。它不是“稍后一定才回调”的接口。故回调可以在外层 Add 之前调用 Stop 或新的 Start。回调停止后外层写回会复活意图；回调建立另一请求后外层覆盖会丢失该较新请求的清理身份。

复现输入：已有 A/Seagull/3，等正式实例 Playing；外层改为 4；旧实例取消回调内对 A Stop，或改为 9。原码可最终留下 4，而不是回调要求的 0/9。此输入是从真实同步调用链推出的可复核用例，本轮尚未在 PIE 执行该交错。

修正入口：`Private/Environment/DivineBeastsEnvironmentFlockPresentation.cpp:19` 的 `StartEnvironmentFlock`。先发布带新 MutationToken 的候选，再申请包与取消旧 RequestId；每个会广播的边界后重新查表并核对 token。Stop 先从映射删除再发取消。StopAll 先清空 Desired/Pack/World，并用门闩拒绝清理期间的新 Start。无跨广播的 TMap 元素引用，无无条件旧快照写回。

同时修正首次资源申请窗口：`ActivateContentPack` 新增可选 Pending 输出，在 `BeginLogicalPreload` 的诊断广播前发布句柄。回调内停止可以真正取消此次加载，而不是因调用方尚未取得返回值而遗漏租约。

### BIRD-02：默认组件身份碰撞，旧组件可以停止另一组件的请求（P2）

原始位置：

- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Environment/DivineBeastsBirdFlockSceneComponent.cpp:33` 使用 `GetOwner()->GetFName()`。
- 同文件原始 40 行按 BoundFlockId 直接 Stop，服务状态没有 Owner/token。

同一个 Actor 的两个组件必定得到相同默认 ID；两个流送 Level 中同名 Actor 也可能只拥有相同短名称。后一组件配置会覆盖前者，任一组件 EndPlay/Stop 又会撤销共用 ID，产生相互消失。

修正：空 ID 使用组件 `GetPathName()`，隔离 Actor 内多组件与 Level 实例；每次新绑定使用注册 token。显式重复 ID 对其他活跃 Owner 或显式服务请求返回错误。组件 Stop 只能停止 token 匹配的条目；失效旧 Owner 的条目可以在新注册时回收。现行无 Owner 的按 ID 停止 API 保留为显式控制接口，场景组件不使用它。

`Tests/VFX/BirdFlock/RunBirdFlockOwnershipReview.py` 已由主任务在真实 PIE 执行，最终 `Saved/Validation/BirdFlockAudit/OwnershipReview.json` 为 passed、9/9，包含两个同 Actor 组件、显式冲突及失败候选 Stop 不影响他人。该报告明确不执行 GPU 验证，也不覆盖分屏玩家更换或流送同名 Actor。

### BIRD-03：换 ID 与非法参数同时发生时，会先删除合法旧群（P2）

原始位置：

- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Environment/DivineBeastsBirdFlockSceneComponent.cpp:34–36`。
- 当 Service/ID 改变时先调用 StopBirdFlock，随后才校验候选 Start 的返回值。

复现输入：A/Seagull/3 已受理，把 FlockId 改为 B，同时 BirdCount=513，或物种改为无效名称。候选被服务拒绝，但 A 已经停止。

修正：当前 `ApplyBirdFlock:31` 和 `ApplyFauna` 先提交合法候选；拒绝时保留旧绑定。成功后先发布新组件绑定，再停止旧 token。Apply/Stop 自身也使用 MutationToken，防止旧实例取消回调内重新 Apply 后，外层又覆盖组件绑定。候选 Count 在调用前取值，不从回调可能修改的属性重新判断本次绑定。

最终真实 Bird/Fauna Owner 报告均验证了“同时换 ID+Count=513”拒绝且保留旧绑定；Fauna 另外验证了越界种子。这证明对应同步校验/受理失败用例的保旧行为；“受理”依旧不代表异步加载或 GPU 播放已经成功，无效物种及重入交错不得由这些用例外推。

### BIRD-04：同一真实世界的 Context 改变会丢失自定义场景意图（P2）

原始位置：

- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/DivineBeastsPresentationClientSubsystem.cpp:269–284`。
- RegionId、ExperienceId、WorldId、WorldGeneration 任一变化都进入 World scope 分支，直接 StopAll；后续只有默认世界鸟群策略重新建立请求。

复现输入：保持 UWorld 相同，在已有自定义组件鸟群时，更新一个有效 ProjectContext 的 RegionId 或 ExperienceId。场景组件仍存活且无第二次 BeginPlay，其意图已被丢弃。此问题可通过公开 C++ API 触发；本轮项目源码检索没有证明已存在生产调用点，所以不把它描述成已观察的线上故障。

修正：`UpdateProjectContext:257` 配对调用共用 `SuspendEnvironmentFlocksForContextChange:365` / `ResumeEnvironmentFlocksAfterContextChange:394`。先给 Bird/Fauna 同时设置暂停门闩，保留当前 Desired，先使旧 RequestId/Pack 身份失效再广播取消。挂起期的新合法意图只排队。最终 Context 提交且新包 Active 后恢复当前映射；取消回调删除的意图不从快照恢复。嵌套 Context 更新用更新 token 和暂停计数协调，仅最外层恢复。

真实 WorldCleanup、跨世界残留、账号退出与 Deinitialize 则清空意图。账号/世界清理的全程门闩覆盖后续目录撤销通知，防止 StopAll 返回后又在同一清理过程被回调重建。相同世界的默认鸟群仅更新仍存在的默认意图，停止回调不会被自动默认策略覆盖。

### BIRD-05：真实异步播放终态未清理项目 RequestId，相同参数无法重试（P2）

原始位置：`Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Environment/DivineBeastsBirdFlockPresentation.cpp:95、106–111`。旧实现只在同步 Submit 拒绝时清除 RequestId；平台已经受理后再异步失败，项目仍认为同一个 RequestId 已提交，相同配置 Apply 会跳过它。

本轮追加修复读取了真实公共合同：`GamePlatformVFXClient/Public/Types/GamePlatformVFXResult.h:31–43` 的 PlaybackSnapshot 保留原中立 RequestId，并以 ParentHandle 标识复合子实例；`Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp:588–600` 的 CompletePlayback 先保存终态快照，再同步广播。项目不需要访问平台私有容器或复制播放器状态。

共用文件的 `BindEnvironmentFlockCompletion:244` 在真正 Submit 之前，通过 IGamePlatformVFXService 注册弱回调。每个项目 LocalPlayer 对当前世界至多保留一个订阅。只有绑定 token 仍当前、弱 World 仍有效、实例句柄属于该世界、ParentHandle 无效、状态为真实顶层终态且 RequestId 精确匹配当前条目时，才清除该 RequestId。由于每次 Submit 创建新的 GUID，旧 Mutation/旧播放的迟到回执不能命中后来请求。回调遍历时只修改当前条目的字段，不触发外部调用、日志、广播或 Apply，所以不会在持有 TMap 迭代器时同步重入。

终态保留 Species/Count/Seed/Origin/Owner/token 意图，并设置 `bAwaitingExplicitRetry`。另一群落的 Apply、重复 Pending 遍历或目录通知不能顺带重试失败群；只有对该 ID 的显式 Apply、新合法配置或新 Context 的恢复才重新准备一次提交。回调本身没有自动重放路径，没有 Tick/计时器/无限重试。Count 查询依旧报告保留的配置数量。

`UnbindEnvironmentFlockCompletion:300` 先清空弱世界、委托句柄和绑定 token，再向公共服务移除委托。Deinitialize、账号 Reset 与真实 WorldCleanup 都调用它；LocalPlayer 已进入新世界时，只撤销真正属于旧世界的订阅，不能解除后来新世界的绑定。

最终真实 Fauna 集成运行已通过销毁 Niagara 组件触发终态，检查无隐式自动重试，并以同参数显式 Apply 恢复到 128 个 GPU 粒子；证据为 `Saved/Validation/Fauna/IntegrationReview.json` 的 terminalCause、terminalExplicitRetry 及主 harness 断言。此结果覆盖共用投影的真实组件销毁通道，不代替异步 Data 加载失败、子实例终态过滤、旧 RequestId/旧 World 回执、其他观察者先 Stop/替换，以及显式重试再次失败等用例。

## 4. 蝴蝶/鱼群项目接入合同

新公开入口在 `DivineBeastsPresentationClientSubsystem.h:101–117`：

- `StartAmbientFauna(HabitatId, SpeciesId, Count, Seed, WorldOrigin, OutError)`
- `StartAmbientFaunaForOwner(HabitatId, SpeciesId, Count, Seed, WorldOrigin, Owner, RegistrationToken, OutError)`
- `StopAmbientFaunaForOwner(HabitatId, RegistrationToken)`
- `ResolveAmbientFaunaContextForOwner(HabitatId, Owner, RegistrationToken, OutContextId, OutError) const`
- `StopAmbientFauna(HabitatId)`、`StopAllAmbientFauna()`、`GetRequestedFaunaCount(HabitatId)`

`ResolveAmbientFaunaContextForOwner` 仅在当前真实 World、未暂停/清理、Owner 存活且属于该 World、注册 token 精确匹配、Count>0 且当前 RequestId 有效时返回 HabitatId。失败先清空 OutContextId；旧 token 不能用于查询后来接管同名 ID 的群落。实现位于 `Private/Environment/DivineBeastsFaunaPresentation.cpp:33`。这个解析只证明项目当前注册已提交，平台 CPU/GPU 状态还需由公共诊断接口实际查询。

`UDivineBeastsFaunaSceneComponent` 默认 SpeciesId=ButterflyAzure、Count=32、Seed=137，ID 为空时使用组件路径。允许 ButterflyAzure/FishSilver，Count=0..512，Seed=0..1000000，坐标必须有限。每个 LocalPlayer 世界中 Bird 目录最多 16 个请求，Fauna 目录最多 16 个请求。数量上限是意图防护，实际可见实例/粒子由统一平台预算、剔除和真实资产决定，不能据此声称满足帧时目标。

- **ButterflyAzure**：逻辑 Definition ID：`presentation.dba.environment.fauna.butterflyazure@1`；正式资产约定：`/DBAPresentationPack_Core/VFX/Environment/Fauna/Definitions/DA_DBA_VFX_Fauna_ButterflyAzure`。
- **FishSilver**：逻辑 Definition ID：`presentation.dba.environment.fauna.fishsilver@1`；正式资产约定：`/DBAPresentationPack_Core/VFX/Environment/Fauna/Definitions/DA_DBA_VFX_Fauna_FishSilver`。

上述资产路径已按实际源资产名修正（Definition 文件名包含 Fauna 前缀，逻辑 ID 不变）。统一资产工作已在真实审核世界通过两种蓝图到 DataLease/平台 VFX/GPU 的自动场景链路；这是运行证据，不代表 Client/Server Cook、发布包或人工美术验收完成。项目编排没有新建另一份路径→定义加载器。

项目中立请求维持 Low、Persistent、Confirmed 的 World 环境表达；提交 `User.FaunaCount` 与 `User.FaunaSeed`。鸟群仍提交 `User.BirdCount`。Fauna ContentPack ID 为 `DBA.Environment.Fauna`，只在本代 Data 预载成功、目录 Active 后从 `HandleLogicalPreloadCompleted` 提交当前意图。两类共用一个私有状态机文件，各自公开入口只做参数类型和目录选择。

组件的 `DisturbFauna(WorldCenter, Radius, Strength, DurationSeconds, OutError)` 通过 `IGamePlatformVFXService::Get(World)->DisturbFauna` 调用平台公共能力，不保存服务裸指针或 Niagara 对象。输入单位为 cm/秒；强度 0..1，时长至多 60 秒，半径须为正数。返回被影响的群数，是纯本地环境行为，不修改捕获、奖励或任何服务器事实。新增模块依赖仅为项目 Client 的 private `GamePlatformVFXClient`，公开头不暴露 VFX 类型。

## 5. 没有作为缺陷直接修复的事项

### 初始 PreCull 推断已通过真实引擎源码缩小

实际 UE 5.8 `Engine/Plugins/FX/Niagara/Source/Niagara/Private/NiagaraWorldManager.cpp:2076–2092` 的位置 PreCull 受 `CanPreCull` 控制；同文件 2192–2195 只对 Deactivate/DeactivateImmediate 返回可初始剔除。正式鸟作者脚本 `Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockSpeciesMonolith.py:22` 设置 DeactivateResume。

因此不能把“出生时远于 35000 cm 会必然被 PreCull 永久丢失”当作该脚本配置下的已确认错误，更不应据此扩大修改唯一 VFX 执行器。实际 uasset 的 EffectType 是否仍与作者合同一致，以及远处出生后靠近的恢复效果，仍须由资产与 PIE 验收确认。

### 首个 LocalPlayer 和旧原型需要资产/运行侧确认

现行场景组件选 GameInstance 的首个 LocalPlayer；自动默认鸟群也只由首个 LocalPlayer 建立，保持世界环境共享。增加分屏玩家不会自动复制默认鸟群，但首个 LocalPlayer 移除/替换、BeginPlay 早于 LocalPlayer 建立时，目前没有新增自动重绑事件，须在真实模式下验收。

旧 ISM 鸟原型仍存在且可被蓝图引用。仅查看正式 C++ 入口不能证明所有地图和蓝图都未引用旧原型；需要 AssetRegistry/引用者与地图审查。未获该证据前不宣称旧原型已完全退出正式资产，也不直接删除可能被现有内容引用的类型。

## 6. 本轮完成的验证

本节已直接回读主任务生成的最终报告。子任务本人只读报告并更新文档，没有启动/关闭 Editor、构建或再次执行 PIE。报告中的实际 GPU 观测与 Owner 请求断言分别列出，不将 CPU 意图数量当作 GPU 计数。

### 6.1 最终实际 PIE 报告

- **`Saved/Validation/BirdFlockAudit/OwnershipReview.json`**：最终结果：`status=passed`，9/9；本次实际覆盖：同 Actor 两组件默认路径独立；换 ID+Count=513 拒绝并保留旧绑定；合法替换；其他 Owner 同名冲突；失败候选 Stop 不影响他人；Count=0；Actor 销毁后的 EndPlay 清理。
- **`Saved/Validation/BirdFlockAudit/FaunaOwnershipReview.json`**：最终结果：`status=passed`，13/13；本次实际覆盖：上述 Owner 生命周期，加 Seed=-1/1000001 拒绝、Seed=0/1000000 和 Count=512 合法边界。
- **`Saved/Validation/Fauna/IntegrationReview.json`**：最终结果：`status=passed`，`errors=[]`；本次实际覆盖：真实 SceneComponent→项目 Presentation→DataLease→平台 VFX→GPU 链路、下列 10 个 Fauna GPU 数量用例、行为与终态恢复检查。
- **`Saved/Validation/BirdFlockAudit/FaunaRegression_20261011T150801/IntegrationReview.json`**：最终结果：`status=passed`，15/15；本次实际覆盖：海鸥/燕子/鹰各 0 / 1 / 32 / 128 / 512、自动场景链路、StopAll 无活动实例与真实世界清理，详见本节飞鸟复跑记录。

本次回读报告的 SHA-256，用于区分后续同名报告覆盖：

- OwnershipReview.json：`3CA49251D7DFED9186AA1B4424FB4917639DEC3E7543374FD04F6ED37F5C57EB`。
- FaunaOwnershipReview.json：`2BF2BD78B158C7BDD94FC22611C2F271D84804FADBFC432E2F9B60CE7F6D3855`。
- Fauna/IntegrationReview.json：`B82EA9531EB399309C5ED6492E96EBE55F5953192AB9E3BC50DFD199905B083C`。
- BirdFlockAudit/FaunaRegression_20261011T150801/IntegrationReview.json：`BED47E58B19E2448906D8C9720CFFB5F1E3A5380ABDB0B177C2280EF6B116F90`。

两个 Owner 报告都明确 `gpuValidation=not_performed`。它们只证明真实组件/请求生命周期；Fauna 和飞鸟的 GPU 证据分别来自独立 IntegrationReview，以下先列 Fauna 用例，再列本轮飞鸟复跑。

- **ButterflyAzure**：请求数量：0 / 1 / 32 / 128 / 512；实际 GPU 观测：0 / 1 / 32 / 128 / 512；结果：5/5。
- **FishSilver**：请求数量：0 / 1 / 32 / 128 / 512；实际 GPU 观测：0 / 1 / 32 / 128 / 512；结果：5/5。

8 个非零用例的 `finite`、`normalizedOrientations`、`validMotionParameters`、`insideHabitatAndObstacles` 均为 true。零数量用例证明 GPU 观测为 0，不额外套用不存在的非零属性检查字段。

同一 IntegrationReview 还记录：

- 自动场景在真实 `UEDPIE_0_L_DBA_FaunaReview` 加载两种项目 Niagara System，`automaticScene.pass=true`。
- 蝴蝶 CPU 生产快照为 simulated=32、perched=1、reservedSlots=8、renderingActive=true；受惊后 perchedAfter=0，检查通过。鱼群 GPU 运动观测为 gpuCount=128、moved=true。
- 实际销毁 Niagara 组件触发终态（`terminalCause=actual Niagara component destruction`）；主 harness `Tools/Unreal/VFX/Fauna/RunFaunaIntegrationReview.py:205–222` 在终态后超过 1 秒的观察窗检查无隐式自动重试、旧请求诊断失效，随后同参数显式 Apply 恢复，报告 `terminalExplicitRetry.gpuObserved=128`、pass=true。
- StopAll 后 `stopAllActiveNiagara=0`，报告 `worldCleanup=true`。这不等同于所有账号退出、跨世界和异常卸载交错都已验证。
- `humanArtAcceptance=pending`；没有从功能检查推导美术验收、性能目标、Client/Server Cook 或最终发布包已通过。

#### 飞鸟既有 GPU 矩阵本轮复跑

主线真实独占 Editor 任务 `wc_job_Sd5ZlUdVUrprHhQ3` 已完成、exit 0。归档报告 `Saved/Validation/BirdFlockAudit/FaunaRegression_20261011T150801/IntegrationReview.json` 与 `VerifiedSummary.json` 已回读：status=passed、15 个用例、automaticScene=true、stopAllActiveNiagara=0、worldCleanup=true，实际报告耗时 34.516 秒。

- 海鸥 Seagull：0 / 1 / 32 / 128 / 512，实际 GPU 观测逐项相等，5/5 通过。
- 燕子 Swallow：0 / 1 / 32 / 128 / 512，实际 GPU 观测逐项相等，5/5 通过。
- 鹰 Eagle：0 / 1 / 32 / 128 / 512，实际 GPU 观测逐项相等，5/5 通过。
- 12 个非零用例均 `finite=true`；三个零数量用例均观测为 0。自动审核场景加载三种正式鸟群 Niagara System，沿真实 SceneBlueprint→项目 Presentation→平台 Presentation→DataLease→VFX→Niagara 链路执行。
- 报告已从既有输出逐字节归档；`VerifiedSummary.json.ReportSHA256` 为 `BED47E58B19E2448906D8C9720CFFB5F1E3A5380ABDB0B177C2280EF6B116F90`，不是沿用历史鸟群通过结论。
- `Saved/Validation/BirdFlockAudit/FaunaRegression_20261011T150801/Inputs.json` 登记既有 `Tools/Unreal/VFX/BirdFlock/RunBirdFlockIntegrationReview.py`、`UnrealEditor-GamePlatformVFXClient.dll` 与 `UnrealEditor-DivineBeastsPresentationClient.dll` 的 SHA-256；`VerifiedSummary.json.UnchangedInputs=true` 确认三项输入在运行前后保持相同。
- 飞鸟报告 `humanAcceptance=pending`。本次通过的是数量、有限值、正式场景链路和 StopAll/世界清理检查，不外推取消回调重入、嵌套 Context、所有失败通道、Cook、性能目标或人工美术验收。

### 6.2 历史失败属于测试桥接/API修正记录

同名 JSON 已由最终成功运行覆盖；以下保留此前实际观察到的失败阶段，不抹去测试迭代，也不把它们计为新增生产缺陷：

1. UE Python 的 bool+FString& 包装成功可以返回空字符串、失败返回 None。旧 accepted 使用 bool('')，把成功误判为失败；现按布尔对象本值、其他对象 result is not None 判断。
2. BeginDeferredActorSpawnFromClass、FinishSpawningActor、AddComponentByClass 等未生成预期 Python 方法名。夹具改用真实 UObject.call_method；先完成 Actor construction，再创建未注册组件、设置初始配置、FinishAddComponent。提前加组件会受到 construction 重建影响。
3. 第二轮曾通过默认 ID 和非法候选拒绝，随后“保留旧绑定”断言失败。锁定引擎 `PyWrapperObject.cpp:992–1048` 的批量 setter 实际统一 PreEditChange/PostEditChange，不是逐字段自动 Apply；不能把“中间有效请求”猜测写成已确认根因。引擎 `ActorComponent.cpp:1311–1327、1432–1444` 存在编辑重注册和 Owner construction 重建路径。
4. 现两份脚本统一使用 `set_runtime_properties`，逐个调用 `set_editor_property(..., PropertyAccessChangeNotifyMode.NEVER)`，再显式 Apply。测试模拟运行态候选赋值，不验证编辑器属性修改事务。所有原有业务断言保留，最终 9/9 与 13/13 通过。

### 6.3 静态证据及尚未覆盖的运行矩阵

- **基线五处控制流、真实 VFX 终态/父子句柄合同复读**：当前证据状态：已执行；边界：可复核原始缺陷因果；不是所有异步交错的运行证明。
- **UE Niagara 初始剔除与 Python 编辑通知源码复读**：当前证据状态：已执行；边界：缩小 PreCull 和属性事务推断；不代替当前 uasset/编辑器事务验收。
- **项目差异 git diff --check、两份 Owner 脚本 AST**：当前证据状态：已通过；边界：静态语法/空白检查；实际 PIE 结果以上述各份报告为准。
- **C++ Editor 构建及已有 Automation**：当前证据状态：主任务独立管理；边界：本节运行报告不替代构建日志或 Automation 结果，不由子任务补造证据。
- **取消回调内 Stop/再次 Start、嵌套 Context 更新、暂停期新意图排队**：当前证据状态：尚未由本节报告覆盖；边界：BIRD-01/BIRD-04 的精确交错仍需 Tests/VFX/BirdFlock/README.md 对应用例。
- **复合子实例终态、旧 RequestId/旧 World 回执、其他观察者先替换、显式重试再次失败**：当前证据状态：尚未由本节报告覆盖；边界：BIRD-05 已有真实组件销毁终态及同参恢复证据，但不等于所有失败通道通过。
- **飞鸟既有 15 组 GPU 矩阵**：当前证据状态：本轮真实复跑 15/15 通过；证据：独立归档 `FaunaRegression_20261011T150801` 的 IntegrationReview、VerifiedSummary 和 Inputs；边界：该结果独立于 Fauna 10 组与 Bird Owner 9 项，不扩展到特殊回调交错、Cook 或人工验收。
- **分屏首玩家更换、流送同名 Actor、PreCull 远近恢复、Client/Server Cook 与人工美术验收**：当前证据状态：本节未提供通过证据；边界：继续按对应真实场景和产物验收。

不得用伪造 UE 头、独立复制的状态机、字段存在或 CPU 配置数量替代真实证据。现有 `DivineBeasts.Presentation.Activation.FailureCancellationAndOldGeneration` 自动化属于 ContentPack 账本取消/旧代回归，不代替真实播放取消的重入用例。

## 7. 变更范围与后续衔接

修改原有 Bird 组件头/实现、Bird 项目入口、项目 Subsystem 头/实现与 Client Build.cs；新增共用 `DivineBeastsEnvironmentFlockPresentation.cpp`、Fauna 项目入口和 Fauna 组件头/实现。源码均位于项目 DivineBeastsPresentationClient。测试和本文为新文件。

未修改平台 VFX 执行器、服务器模块、总体目录规划或已有他人工作。新路径由主任务统一登记。共享 Subsystem.h/cpp 已交回主任务继续整体验收；后续提交以最新工作区为准，不覆盖并行作者的目录/平台/资产改动。
