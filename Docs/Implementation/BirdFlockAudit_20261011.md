# 神兽联盟正式飞鸟审查与环境群落接入记录

日期：2026-10-11  
审查基线：`bd106b3259c1a1917c0ece46d3ec2a8edd227fd4`  
工作区：`E:/work/2026/DivineBeastsWorkspace`  
引擎：`D:/UnrealEngine-5.8.0-release`

## 1. 结论与证据边界

本轮确认五个项目编排缺陷：取消通知重入会覆盖较新的意图；组件默认身份碰撞及按 ID 误停；换 ID 的非法候选会先破坏合法旧绑定；同一 UWorld 的 Context 变化会丢弃自定义场景意图；实际异步播放终态未反馈到项目 RequestId，导致相同参数无法重试。已在项目层实现相应修正，并将修正后的同一套事务用于蝴蝶与鱼群。

这是源码审查与实现记录。本轮子任务没有启动/关闭编辑器、构建 C++、执行 PIE、运行图形验证或提交 Git。真实 UE 编译、实际异步通知交错、资源登记及 GPU 效果须由统一验收阶段补齐。下文明确区分已观察的源码事实与尚待运行的用例，不沿用旧文档中的“通过”作为本轮证据。

正文中“原始行”指上述 Git 基线；“当前行”指本轮项目编排交回时的源码快照，后续合并可按函数名定位。

## 2. 正式链路与所有权

正式场景组件只把物种、数量和世界锚点转为项目中立请求。项目目录用 `GetBirdFlockTag/BuildBirdFlockFragment` 或 `GetFaunaTag/BuildFaunaFragment` 选择逻辑 Definition ID；既有 ContentPack 事务通过 GamePlatformData 申请真实租约，全部必需定义就绪后才注册目录。平台 Presentation 解析中立请求，唯一 GamePlatformVFX 世界执行器拥有 Niagara 实例和播放预算。

本轮没有增加 Niagara 播放器、逐鸟/逐鱼 Actor、项目资源加载器或新 VFX 插件。新组件只在 BeginPlay、显式 Apply、编辑器 PIE 属性变更和 EndPlay 事件中工作，没有自身 Tick。

每个当前意图包含：

| 字段 | 含义 |
| --- | --- |
| ID | Bird 的 FlockId 或 Fauna 的 HabitatId；两个类型的目录分别命名，不相互占位 |
| Owner + RegistrationToken | 场景拥有者与注册代次；空 token 条目属于显式服务请求 |
| MutationToken | 每次配置变更的意图代次；外层操作在同步广播后必须仍匹配才能继续 |
| RequestId | 当前提交到平台的中立请求身份；挂起/停止在广播取消前先撤销，真实顶层终态精确匹配后清除 |
| bAwaitingExplicitRetry | 终态后保留意图但暂停隐式再提交；该 ID 显式 Apply 或新 Context 恢复重新准备一次 |
| World | 该意图绑定的真实 UWorld；过期世界的意图不迁移到后来世界 |
| Species/Count/Seed/Origin | 配置值；Count 是请求数量，不是可见粒子计数 |

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

真实组件回归准备于 `Tests/VFX/BirdFlock/RunBirdFlockOwnershipReview.py`，包括两个同 Actor 组件、显式冲突及失败候选 Stop 不影响他人。脚本尚未执行 PIE。

### BIRD-03：换 ID 与非法参数同时发生时，会先删除合法旧群（P2）

原始位置：

- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Environment/DivineBeastsBirdFlockSceneComponent.cpp:34–36`。
- 当 Service/ID 改变时先调用 StopBirdFlock，随后才校验候选 Start 的返回值。

复现输入：A/Seagull/3 已受理，把 FlockId 改为 B，同时 BirdCount=513，或物种改为无效名称。候选被服务拒绝，但 A 已经停止。

修正：当前 `ApplyBirdFlock:31` 和 `ApplyFauna` 先提交合法候选；拒绝时保留旧绑定。成功后先发布新组件绑定，再停止旧 token。Apply/Stop 自身也使用 MutationToken，防止旧实例取消回调内重新 Apply 后，外层又覆盖组件绑定。候选 Count 在调用前取值，不从回调可能修改的属性重新判断本次绑定。

这保证同步校验/受理失败保留旧配置；“受理”依旧不代表异步加载或 GPU 播放已经成功。

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

本轮完成的是源码实现及真实合同复读，尚未执行实际失败注入或证明 GPU 恢复成功。运行验收必须覆盖顶层失败后相同参数重试、子实例终态过滤、旧 RequestId/旧 World 回执、其他观察者先 Stop/替换意图，以及一次显式重试仍失败后保持等待。

## 4. 蝴蝶/鱼群项目接入合同

新公开入口在 `DivineBeastsPresentationClientSubsystem.h:101–117`：

- `StartAmbientFauna(HabitatId, SpeciesId, Count, Seed, WorldOrigin, OutError)`
- `StartAmbientFaunaForOwner(HabitatId, SpeciesId, Count, Seed, WorldOrigin, Owner, RegistrationToken, OutError)`
- `StopAmbientFaunaForOwner(HabitatId, RegistrationToken)`
- `ResolveAmbientFaunaContextForOwner(HabitatId, Owner, RegistrationToken, OutContextId, OutError) const`
- `StopAmbientFauna(HabitatId)`、`StopAllAmbientFauna()`、`GetRequestedFaunaCount(HabitatId)`

`ResolveAmbientFaunaContextForOwner` 仅在当前真实 World、未暂停/清理、Owner 存活且属于该 World、注册 token 精确匹配、Count>0 且当前 RequestId 有效时返回 HabitatId。失败先清空 OutContextId；旧 token 不能用于查询后来接管同名 ID 的群落。实现位于 `Private/Environment/DivineBeastsFaunaPresentation.cpp:33`。这个解析只证明项目当前注册已提交，平台 CPU/GPU 状态还需由公共诊断接口实际查询。

`UDivineBeastsFaunaSceneComponent` 默认 SpeciesId=ButterflyAzure、Count=32、Seed=137，ID 为空时使用组件路径。允许 ButterflyAzure/FishSilver，Count=0..512，Seed=0..1000000，坐标必须有限。每个 LocalPlayer 世界中 Bird 目录最多 16 个请求，Fauna 目录最多 16 个请求。数量上限是意图防护，实际可见实例/粒子由统一平台预算、剔除和真实资产决定，不能据此声称满足帧时目标。

| 物种 | 逻辑 Definition ID | 正式资产约定 |
| --- | --- | --- |
| ButterflyAzure | `presentation.dba.environment.fauna.butterflyazure@1` | `/DBAPresentationPack_Core/VFX/Environment/Fauna/Definitions/DA_DBA_VFX_ButterflyAzure` |
| FishSilver | `presentation.dba.environment.fauna.fishsilver@1` | `/DBAPresentationPack_Core/VFX/Environment/Fauna/Definitions/DA_DBA_VFX_FishSilver` |

资产的实际创建、Data 扫描/PrimaryAssetId 登记和运行资源验收属于统一资产工作。本轮项目编排不新建另一份路径→定义加载器。

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

| 检查 | 结果 | 能证明什么 |
| --- | --- | --- |
| 基线五处控制流复读 | 已执行 | 缺陷的输入、因果和原始位置可复核 |
| 真实 UE Niagara 初始剔除源码复读 | 已执行 | DeactivateResume 与可 PreCull 模式的区别；不能证明 uasset 当前配置 |
| 公共 VFX 终态/父子句柄与同步广播源码复读 | 已执行 | 原中立 RequestId 与 ParentHandle 的真实语义，以及不在投影回调内调用外部代码的必要性 |
| 真实 UE Python 批量属性写入源码复读 | 已执行 | 集成脚本的“同时换 ID+非法参数”输入不会通过逐属性中间通知拆开 |
| 项目 C++ 差异 `git diff --check` | exit 0 | 补丁没有 Git 空白错误；有工作区既有 LF/CRLF 转换提示 |
| 旧鸟群专用状态标识残留检查 | 未发现旧字段/函数残留 | 抽取后没有遗留旧状态变量调用；不是编译通过证据 |
| 两个真实组件集成脚本 AST | 通过 | Python 语法有效，使用实际 UE 附带解释器；没有执行 import unreal/PIE |
| C++ Editor 构建及已有 Automation | 本子任务未执行 | 必须由统一阶段实际构建、运行 |
| 取消/Context 回调交错、资源加载、GPU/预算画面 | 本子任务未执行 | 完整输入矩阵见 Tests/VFX/BirdFlock/README.md |

不得用伪造 UE 头、独立复制的状态机、静态字段存在检查或 CPU 意图数量替代真实 UE 编译、实际资源和 GPU 运行证据。现有 `DivineBeasts.Presentation.Activation.FailureCancellationAndOldGeneration` 自动化继续作为 ContentPack 账本取消/旧代回归，不能代替真实播放取消的重入用例。

## 7. 变更范围与后续衔接

修改原有 Bird 组件头/实现、Bird 项目入口、项目 Subsystem 头/实现与 Client Build.cs；新增共用 `DivineBeastsEnvironmentFlockPresentation.cpp`、Fauna 项目入口和 Fauna 组件头/实现。源码均位于项目 DivineBeastsPresentationClient。测试和本文为新文件。

未修改平台 VFX 执行器、服务器模块、总体目录规划或已有他人工作。新路径由主任务统一登记。共享 Subsystem.h/cpp 已交回主任务继续整体验收；后续提交以最新工作区为准，不覆盖并行作者的目录/平台/资产改动。
