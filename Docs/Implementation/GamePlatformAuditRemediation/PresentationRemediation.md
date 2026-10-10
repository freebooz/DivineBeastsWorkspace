# 表现组整改交付报告（2026-10-09）

本组源码重新冻结；本组授权范围源码工作已交付，统一UE/资源/服务器验收仍待执行；未提交或推送。

工作区：`C:/Users/Freebooz/.codex/worktrees/gameplatform-review-fixes/DivineBeastsWorkspace`。分支：`codex/gameplatform-audit-fixes-20261009`。整合HEAD：`0f6afcb571185626487c4c5da3ee750a52b71bb0`。

## 范围与真实结论

五平台表现插件、MobaPresentation、DBAClient两个Presentation模块的人工源码/Private测试/局部CMake/中文文档。保留整合main的UI/原型资产/预览动画/输入更新。

Camera、纯内容/二进制资产、DBAClient Application/Input/UI业务模块、全局.uplugin/.uproject/Targets/Architecture门禁和根规划归其他执行者。ModifiedFiles包含共享目录内其他人改动时不归本组；本报告排除Surface.uplugin。

本轮没有制造或修改uasset、umap、Niagara、MPC、材质或UI视觉资产，没有运行UBT、Cook、Monolith或发布。只读核对锁定UE5.8.0真实源码。

## 逐项当前源复核与整改

| 发现 | 当前状态 | 结果与未验收 |
| --- | --- | --- |
| F01 目录精确语义与显式父回退优先级 | 已合入旧修复复核；六字段具体度补修；UE待验收 | 资格仍检查全部合同字段；排名只统计Hero/Ability/Skin/World/Platform/Quality六个等值项。旧Specificity反射字段保留并标为弃用，不参与排名。 **未验收：**实际Registry UE用例、真实目录资产加载及父标签层级运行尚未执行。 |
| F02 跨片段同局部EntryId歧义及发布前冲突 | 旧消歧修复复核；诊断与只读预检补齐；UE待验收 | ResolvedEntry保存目录/Owner/Revision/匹配语义及所有最高同键候选诊断。PreflightCatalogFragment检查片段内部同键、已注册片段资格交集和容量/身份；工具Register兼容入口仍允许注册后由Resolver明确拒绝歧义。预检是逻辑资格检查，非资产验收。 **未验收：**UE Automation未执行；真实包资产拥有者与版本仍须Data/AssetRegistry验证。 |
| F03 VFX兼容Resolver采用统一P13排序 | 生产评分回归红转绿；UE Registry待验收 | 先资格，再精确/显式逐级父语义、Scope、六等值项、显式Priority；Context集合不加分。typed Hero/Ability/Skin/World传入兼容请求。ContextTier仅保留内部兼容值，不影响比较。Catalog.Priority+Entry.Priority使用64位加法，弃用百万权重。 **未验收：**UE加载真实StartupCatalogs/父标签/画质变体尚未执行；旧兼容目录显式Priority需迁移复核。 |
| F04 Composite依赖与根树清理预算 | 旧依赖门禁复核；根树生命周期补齐；UE待验收 | 按根请求累计全部后代出生数和深度，子结束不退还总树预算；必需子失败取消根及兄弟。清理重入护栏防重复释放；父已失效则新计时器立即清理。Preload遍历RequiredSteps并去重、有限深度/定义数量。 独立复核补修：Run/子受理/Timer登记返回bool，全部必需步骤预检，拒绝Failed并撤销整树。 **未验收：**真实A到B到A资产图、嵌套后代上限、Niagara子启动失败/世界退出及计时器运行未执行；不能称资产无环验收通过。 |
| F05 可选Niagara变体及FallbackDefinition真实回退 | 源码回退与资源租约已实施；实际资产待验收 | 定义元数据空Bundle租约与本次选中Niagara/EffectType/PreloadAssets资源租约分离。选中变体失败先尝试同定义基础，再经Data读取FallbackDefinition元数据及合法ID；不LoadSynchronous、不另建AssetManager。Visited和MaxDefinitionFallbackDepth防环，缺回退保持明确Failed。停止/销毁清理各自租约，权威事实不受影响。 **未验收：**没有真实Definition/Niagara/EffectType/关键预警基础资产，回退实播、多世界Data释放、原生池/GC仍未执行。 |
| F06 SFX总受理容量预留 | 旧修复已覆盖；原生回归通过；UE待验收 | 保留既有总额策略，终态/取消继续释放对应预留一次。 **未验收：**真实255 Active加128 Pending批量完成、抢占、并发、5v5 Audio Insights未执行。 |
| F07 SFX起播完成监听窗口 | 旧先监听修复及真实引擎源码已复核；实播待验收 | 保留先监听后播放，真实UE5.8.0 PlaybackCompleted实现证实自然完成Finished先于Stopped，而失败起播跳过Finished后仍发Stopped；未被Finished回收且非显式Stop的Stopped按真实失败清理。移除无用bStarted。 **未验收：**极短SoundWave/MetaSound、并发拒绝、虚拟化、Owner销毁真实音频仍未测试。 |
| F08 受理后真实终态与桥接拒绝 | 公开终态合同及去重失败已实施；UE待验收 | VFX/SFX公开GetPlaybackSnapshot与完成订阅，保存具体Code、RequestId（VFX ParentHandle区分根子），终态最多512项。清理后一次通知；结束确认保留真实失败。Provider返回实际受理或已处理取消。DBA两个提交入口与Moba同步非Submitted撤销本次去重，重复未受理不伪成功。Submitted/Queued仅表示受理。 独立复核补修：新增OwnsHandle精确身份查询，Cleanup/Stop/世界退出按账本清理，CleaningInstances及终态仅完成一次。 独立复核补修：捕获World和生命周期代次，Stop及执行返回后复核；关闭返回InvalidWorld。 独立复核补修：强引用服务/World，Stop后核closing/World/Generation；Deinitialize幂等。 **未验收：**UE Automation与真实异步失败尚未执行。普通目录撤销使未来映射不可见，已播放实例保留自有租约到结束；ContentRevoked enum预留但该普通注销策略不生成该终态。 |
| F09 Surface实际MPC与材质链缺失 | 实际内容缺口保留；本轮不生成资产 | 机制切片和缺配置失败在中文文档保留，不造Niagara/MPC/材质/UI占位资产。 **未验收：**真实MPC与9路径材质合同、世界消费者、Shader编译、设备GPU成本/美术验收待内容阶段。 |
| F10 Surface异步Data租约与稳定失败 | 源码已实施；新旧代/取消回归待UE | 使用Data既有普通资源World租约；刷新先取消释放自身旧代，成功回调再核World、generation及完整lease身份。Pending新增枚举尾项，LastBindingResult稳定保存缺资源/参数失配；普通重复状态不重试，只显式Refresh换代。 **未验收：**真实Data资产成功/失败及MPC参数写入UE运行未执行；Surface.uplugin的Data依赖由主执行者统一修改，非本组改动。 |
| F11 UI实际离栈释放与初始化/广播重入 | 旧资源所有权修复复核；事件时序补齐；UE待验收 | CommonUI SlateRelease也触发真实移除核对。激活先建立事件绑定后BeginPage并复核自闭；失活先解绑/结束旧代再通知，重入新代不能被旧EndPage结束。ViewModel.EndPage在OnPageEnded前使旧代失效。 独立复核补修：强引用原/新Root、布局代次和closing核验每个外部边界；关闭后不重新发布。 独立复核补修：先发布选择及绑定，旧End/newBegin/BindUI/Refresh捕获VM/页面/激活/选择代次；嵌套选择优先。 **未验收：**真实CommonUI Stack覆盖/返回/GC软预载、初始化自闭、广播重入、焦点与多LocalPlayer未执行。无Widget视觉资产改动，未调用Monolith。 |
| F12 MOBA事实类型化上下文迁移 | 源码已实施；UE映射回归待执行 | 显式复制事实源HeroDefinitionId/AbilityId为FName，ArenaModeId/AvatarGeneration/WorldGeneration进入Request.Context；MOBA事实自身世界代次先校验；平台顶层/Context世代由同次Submit刷新并补齐。空字符串None，不推断观察者英雄。 独立复核补修：MOBA自身事实先核World；中立世代0交同次平台Submit刷新后补齐top/context。 **未验收：**真实英雄/技能Definition映射、旁观他人及多人事实未执行。 |
| F13 迟到Pawn/GameState与重生订阅 | 引擎事件接线已实施；UE/联机待验收 | 订阅LocalPlayer ControllerChanged、Controller NewPawnNotifier、World GameStateSet/ActorSpawned，相关出生下一调度轮补一次查找；无永久Tick。替换Controller/Combat/Arena先解绑旧事件，世界清理全部解绑。 独立复核补修：持有PendingBindingRefreshTimer，旅行/关闭取消，回调核closing/绑定代次/World。 **未验收：**未执行UE Automation/迟到复制联机；任意更晚动态插入Combat而无组件就绪事件仍需要既有RefreshBindings，不宣称全动态自动就绪。 |
| F14 内容激活预载成功后原子发布 | 真实Data事务源码已实施；UE资源/回调待验收 | Pending独立Loading，定义及声明必需Bundle实际Data成功后验证Provider声明DefinitionClass、执行逻辑Preflight再一次注册/Active。失败取消整组自有租约，先回滚再广播；旧代/账户/世界/跨Scope拒绝。Active撤销先取消映射再释放自身。状态查询和终态有界128。 **未验收：**真实Data定义/Bundle/资产拥有者、回调重入、完整成功发布未执行。当前内容事务预检声明VFXRuntime/SFXRuntime必需资源；可选上下文变体预热仍须明确项目选择合同，不能称全部可选Ready。 |
| F15 合法默认ID与实际资源缺配置 | 格式/失败合同已修；真实默认资源仍缺 | 两默认ID改为presentation.dba.world.interaction.committed.default@1及presentation.dba.village.guidance.ready.default@1。包所有逻辑ID经现有GamePlatformId验证；默认目录等待真实预载成功才发布，错误可查询，不把合法ID当真实播放成功。 **未验收：**DBAPresentationPack_Core及两真实Definition/基础Niagara没有交付，因此默认世界交互/教学反馈视觉尚未验收。 |
| F16 Common/Hero服务器装配及表现剥离 | 交主执行者统一装配；本组未宣称通过 | 本组未更改.uproject/Targets/全局描述或Cook规则；服务器闭包和表现剥离由主执行者整改及同HEAD统一验证。 **未验收：**Editor/Client/Server构建、干净Cook/Stage、最终容器Common/Hero/UI/VFX/Surface与最小权威动画审核仍待主执行者证据。 |
| F17 本轮中文职责/API与迁移说明 | 本轮触及范围已补伴随说明；全量存量不在本组验收 | 63个本轮修改/新增源码有中文职责/端侧/所有权文件头和七份伴随中文文档；说明调用前提、字段/单位、错误/取消、线程、代次、资源租约、重入及兼容迁移。README链接同步；ASCII路径与稳定反射/挂载点保留。 **未验收：**公开Context/Moba原有未触及Types、Camera及各内容包历史说明仍需后续逐项中文审计；主执行者统一根目录规划和全局交付说明。 |

以下记录旧整合已覆盖项与本轮证据，避免把已有修复再次算成新增完成：

### F01 目录精确语义与显式父回退优先级

整合HEAD已按SemanticRank/SemanticDepth先比较，bAllowParentFallback已有显式开关；本轮未重复实现该旧修复。发现Query.GetSpecificity仍计十一字段并叠加手填Specificity，与P13六项不一致。

源码复核实际ResolveCatalog顺序；新增ExactAndFragmentConflict及PreflightConflictingQualification UE用例。Presentation原生策略Debug/Release退出0。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationCore/Private/GamePlatformPresentationCatalog.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationCore/Public/GamePlatformPresentationCatalog.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/GamePlatformPresentationClientSubsystem.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/Tests/GamePlatformPresentationRegistryTests.cpp`

### F02 跨片段同局部EntryId歧义及发布前冲突

当前HEAD不再以EntryId不同才算歧义；局部同名跨片段的旧缺陷已被整合修复。

新增同名跨片段、完全同键、Hero与Ability可相交资格、互斥Hero、六字段具体度UE回归源码。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Public/GamePlatformPresentationClientSubsystem.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/GamePlatformPresentationClientSubsystem.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationCore/Public/GamePlatformPresentationCatalog.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/Tests/GamePlatformPresentationRegistryTests.cpp`

### F03 VFX兼容Resolver采用统一P13排序

兼容目录仍存在ContextTier/Specificity优先Scope，并将集合标签数量加入具体度，未被旧修复覆盖。

真实首次NativeVFX测试红灯F03: scope must precede context specificity，原工具调用退出1；修复后独立生产score头的Debug/Release CTest均退出0。Private Registry用例已补，未执行。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Resolution/GamePlatformVFXCatalogRegistry.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Resolution/GamePlatformVFXCatalogScore.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Tests/GamePlatformVFXCatalogScoreNativeTests.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Tests/GamePlatformVFXResolverTests.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Catalogs/GamePlatformVFXCatalogTypes.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Types/GamePlatformVFXRequest.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Tests/CMakeLists.txt`

### F04 Composite依赖与根树清理预算

旧整合已经要求Step列入RequiredDefinitions，Data依赖图可看到Composite边；未重复重写该校验。仍需根树总后代预算与强制子失败传播。

实际CompositeDefinition当前ValidateDefinition及世界执行/清理源码复核；原生既有策略Debug/Release通过。 新回归：ResourceCancellationAndTerminalOnce中的深度/容量拒绝（UE未执行）。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Composite/GamePlatformVFXCompositeRunner.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Composite/GamePlatformVFXCompositeRunner.h`

### F05 可选Niagara变体及FallbackDefinition真实回退

FallbackDefinition旧合同没有运行读取路径；定义Runtime Bundle整体加载会使缺可选变体阻断基础尝试。

资源等待取消、旧回调、失败、WorldDestroyed和预载非Ready的Private/Tests源码已加入；源码人工阅读清理路径。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Execution/GamePlatformVFXNiagaraExecutor.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Definitions/GamePlatformVFXDefinition.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Settings/GamePlatformVFXSettings.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Tests/GamePlatformVFXResourceOwnershipTests.cpp`

### F06 SFX总受理容量预留

当前HEAD使用Pending+Active合计预留，255活跃时不能再接纳128等待；没有重复实现旧修复。

现有生产BudgetPolicy及Native SFX Debug/Release CTest均退出0。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.cpp`

### F07 SFX起播完成监听窗口

整合HEAD已用CreateComponent(bPlay=false,bAutoDestroy=false)，在Play前登记账本及Finished/PlayState回调。

只读核对F:/UnrealEngine-5.8.0-release/Engine/Source/Runtime/Engine/Private/Components/AudioComponent.cpp:1203及AudioDevice.cpp的CreateComponent参数；Build.version为5.8.0。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Tests/GamePlatformSFXLifecycleTests.cpp`

### F08 受理后真实终态与桥接拒绝

旧服务只有Queued及IsActive；桥接可忽略实际Play拒绝，事实去重会把首次未受理重复升级为Submitted。

VFX资源/终态、SFX生命周期、DBA世界交互/合法教学反馈及MOBA重复ProviderMissing回归源码已加入；最末修改后diff --check退出0和54源码辅助扫描通过。 新回归：ResourceCancellationAndTerminalOnce；AbstractConstraintMissingDefinitionAndNaturalLeaseRelease（UE未执行）。 新回归：ResourceCancellationAndTerminalOnce；真实Data接纳总数不增加（UE未执行）。 新回归：StoppedAndTerminal（UE未执行）。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Interfaces/GamePlatformVFXService.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Types/GamePlatformVFXResult.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Public/Interfaces/IGamePlatformSFXService.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Public/Types/GamePlatformSFXTypes.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.cpp`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/DivineBeastsPresentationClientSubsystem.cpp`
- `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationClient/Private/MobaPresentationClientSubsystem.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Instances/GamePlatformVFXInstanceRegistry.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Instances/GamePlatformVFXInstanceRegistry.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Tests/GamePlatformVFXDataIntegrationTests.cpp`

### F09 Surface实际MPC与材质链缺失

仍无实际MPC/Material/Function完整资源链；现有路径合同不证明资源可见效果。

范围文件清单复核；Surface静态检查退出0只证明源码端侧/合同。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Docs/AuditRemediation-2026-10-09.md`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/README.md`

### F10 Surface异步Data租约与稳定失败

旧实现Initialize/Apply/Refresh同步加载，失败被Unchanged覆盖并重复尝试。

Private/Tests有失败不被Unchanged覆盖、旧成功不得复活、取消/世界退出回归；Surface静态脚本退出0；无LoadSynchronous资源备用。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/GamePlatformSurfaceClient.Build.cs`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Private/Subsystems/GamePlatformSurfaceWorldSubsystem.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Private/Subsystems/GamePlatformSurfaceWorldSubsystem.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Private/Tests/GamePlatformSurfaceBindingTests.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Public/Types/GamePlatformSurfaceTypes.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Private/Types/GamePlatformSurfaceTypes.cpp`

### F11 UI实际离栈释放与初始化/广播重入

旧整合已在AddWidget同步自闭后复核，Covered inactive仍持租约，实际WidgetList移除才释放。保留这些修复。

已有层栈/自闭回归及本轮CloseObserverKeepsNewPage源码；UI原生策略Debug/Release CTest退出0。 新回归：RootReplacementAndFailureNotificationClose（UE未执行）。 新回归：ViewModelChangeSelfCloseAndNestedChange（UE未执行）。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Manager/GamePlatformUIManagerSubsystem.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Core/GamePlatformActivatableWidgetBase.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Screens/GamePlatformUIScreen.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/ViewModels/GamePlatformViewModelBase.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Public/ViewModels/GamePlatformViewModelBase.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Tests/GamePlatformUILifecycleRegressionTests.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Public/Core/GamePlatformActivatableWidgetBase.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Public/Manager/GamePlatformUIManagerSubsystem.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Tests/GamePlatformUIReentryFixture.h`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Tests/GamePlatformUIReentryTests.cpp`

### F12 MOBA事实类型化上下文迁移

RequestBuilder丢失Fact Context的Hero/Ability/ArenaMode/AvatarGeneration。

Runtime typed source字段测试源码已补，RequestBuilder逐项阅读。 新回归：FirstFactAfterTravelUsesCurrentCoordinatorGeneration（真实两个World/LocalPlayer子系统）（UE未执行）。

关联文件：
- `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationRuntime/Private/MobaPresentationRequestBuilder.cpp`
- `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationRuntime/Private/Tests/MobaPresentationRuntimeTests.cpp`

### F13 迟到Pawn/GameState与重生订阅

旧服务只有初始化/Reset/PostLoadMap查一次Pawn/Combat。

真实UE5.8.0公开API只读核对；Transient Controller Possess/UnPossess/重生Private测试已加入。 新回归：LatePawnAndRespawnBindings中的取消Timer及旧回调拒绝（UE未执行）。

关联文件：
- `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationClient/Private/MobaPresentationClientSubsystem.cpp`
- `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationClient/Public/MobaPresentationClientSubsystem.h`
- `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationClient/Private/Tests/MobaPresentationPawnBindingTests.cpp`

### F14 内容激活预载成功后原子发布

旧BeginLogicalPreload仅诊断广播，无Data消费者，先发布目录再宣称Active。

失败回滚、取消后迟到成功、旧代、跨Scope Private回归源码。公共Provider类型通过向下声明注入，Presentation未反向依赖VFX/SFX。

关联文件：
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/DivineBeastsPresentationClientSubsystem.cpp`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Public/DivineBeastsPresentationClientSubsystem.h`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Tests/DivineBeastsPresentationActivationTests.cpp`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/DivineBeastsPresentationClient.Build.cs`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Public/ContentPacks/DivineBeastsPresentationContentPack.h`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/ContentPacks/DivineBeastsPresentationContentPack.cpp`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Public/GamePlatformPresentationClientSubsystem.h`

### F15 合法默认ID与实际资源缺配置

旧默认ID缺@version，且没有相应真实公共表现包定义。

Runtime默认ID/包合同测试源码，运行默认预载明确错误路径。

关联文件：
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Catalog/DivineBeastsPresentationProjectCatalog.cpp`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/ContentPacks/DivineBeastsPresentationContentPack.cpp`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Tests/DivineBeastsPresentationRuntimeTests.cpp`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/DivineBeastsPresentationRuntime.Build.cs`
- `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/DivineBeastsPresentationClientSubsystem.cpp`

### F16 Common/Hero服务器装配及表现剥离

全局启用/目标/Pak声明不属于本组独占写入范围。

本组源码保持平台/MOBA/项目单向边界，未启动并行UBT。

### F17 本轮中文职责/API与迁移说明

原审查含许多未改公开字段及Camera/内容README存量缺口；本组不修改Camera/内容资产或根规划。

SourceReview辅助文件头/分隔符63文件通过；人工阅读重点路径与正式说明同步。扫描不是中文人工语义审核，更不是全仓合规证据。

关联文件：
- `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Docs/AuditRemediation-2026-10-09.md`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Docs/AuditRemediation-2026-10-09.md`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Docs/AuditRemediation-2026-10-09.md`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Docs/AuditRemediation-2026-10-09.md`
- `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Docs/AuditRemediation-2026-10-09.md`
- `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Docs/AuditRemediation-2026-10-09.md`
- `Game/Plugins/DivineBeasts/DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md`

## 独立复核八项Important补修

| 问题 | 修复与回归 | 状态 |
| --- | --- | --- |
| R01 自然Niagara完成先失活，IsActive使Cleanup/Stop拒绝真实账本。 | 新增OwnsHandle精确身份查询，Cleanup/Stop/世界退出按账本清理，CleaningInstances及终态仅完成一次。 回归：ResourceCancellationAndTerminalOnce；AbstractConstraintMissingDefinitionAndNaturalLeaseRelease | 源码修复/UE回归未执行 |
| R02 替Root ClearScreenOwnership同步Closed回调关闭后RootLayout为空。 | 强引用原/新Root、布局代次和closing核验每个外部边界；关闭后不重新发布。 回归：RootReplacementAndFailureNotificationClose | 源码修复/UE回归未执行 |
| R03 VFX Corrected Stop终态回调关闭后仍Reserve/Acquire。 | 捕获World和生命周期代次，Stop及执行返回后复核；关闭返回InvalidWorld。 回归：ResourceCancellationAndTerminalOnce；真实Data接纳总数不增加 | 源码修复/UE回归未执行 |
| R04 SFX Corrected Stop通知关停后仍创建替换事务。 | 强引用服务/World，Stop后核closing/World/Generation；Deinitialize幂等。 回归：StoppedAndTerminal | 源码修复/UE回归未执行 |
| R05 InitializeActivatableViewModel新BeginPage同步自闭后重新绑定。 | 先发布选择及绑定，旧End/newBegin/BindUI/Refresh捕获VM/页面/激活/选择代次；嵌套选择优先。 回归：ViewModelChangeSelfCloseAndNestedChange | 源码修复/UE回归未执行 |
| R06 CompositeRunner void早退、必需子伸缩拒绝静默跳过而父Playing。 | Run/子受理/Timer登记返回bool，全部必需步骤预检，拒绝Failed并撤销整树。 回归：ResourceCancellationAndTerminalOnce中的深度/容量拒绝 | 源码修复/UE回归未执行 |
| R07 MOBA取旅行前Coordinator缓存世代，首次新事实StaleWorld。 | MOBA自身事实先核World；中立世代0交同次平台Submit刷新后补齐top/context。 回归：FirstFactAfterTravelUsesCurrentCoordinatorGeneration（真实两个World/LocalPlayer子系统） | 源码修复/UE回归未执行 |
| R08 未持有/取消next-tick接线，Deinitialize后重新绑定。 | 持有PendingBindingRefreshTimer，旅行/关闭取消，回调核closing/绑定代次/World。 回归：LatePawnAndRespawnBindings中的取消Timer及旧回调拒绝 | 源码修复/UE回归未执行 |

ExpectedClass允许抽象IsA约束的Data源码由主执行者修改；本组只用公开真实服务增加Play/Preload缺定义失败及真实普通资源租约释放回归，UE未执行。没有改Data Private或伪造资源。

## 实际执行检查

4个原生策略测试目标各Debug/Release运行一次，共8次CTest调用各1个注册测试，全部退出0；不能折算成UE/资产/网络用例通过数。三静态脚本、diff及63源码辅助检查退出0。独立复核新增的8项生命周期/跨模块UE回归未执行，没有伪称红转绿。

| 命令 | 退出码 | 证据/性质 |
| --- | --- | --- |
| `cmake -S Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Tests -B Saved/PP20261009` | 0 | `GamePlatformPresentation-Native.log`；NativePolicyOnly |
| `cmake --build Saved/PP20261009 --config Debug` | 0 | `GamePlatformPresentation-Native.log`；NativePolicyOnly |
| `ctest --test-dir Saved/PP20261009 -C Debug --output-on-failure` | 0 | `GamePlatformPresentation-Native.log`；NativePolicyOnly |
| `cmake --build Saved/PP20261009 --config Release` | 0 | `GamePlatformPresentation-Native.log`；NativePolicyOnly |
| `ctest --test-dir Saved/PP20261009 -C Release --output-on-failure` | 0 | `GamePlatformPresentation-Native.log`；NativePolicyOnly |
| `cmake -S Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Tests -B Saved/PS20261009` | 0 | `GamePlatformSFX-Native.log`；NativePolicyOnly |
| `cmake --build Saved/PS20261009 --config Debug` | 0 | `GamePlatformSFX-Native.log`；NativePolicyOnly |
| `ctest --test-dir Saved/PS20261009 -C Debug --output-on-failure` | 0 | `GamePlatformSFX-Native.log`；NativePolicyOnly |
| `cmake --build Saved/PS20261009 --config Release` | 0 | `GamePlatformSFX-Native.log`；NativePolicyOnly |
| `ctest --test-dir Saved/PS20261009 -C Release --output-on-failure` | 0 | `GamePlatformSFX-Native.log`；NativePolicyOnly |
| `cmake -S Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Tests -B Saved/PU20261009` | 0 | `GamePlatformUI-Native.log`；NativePolicyOnly |
| `cmake --build Saved/PU20261009 --config Debug` | 0 | `GamePlatformUI-Native.log`；NativePolicyOnly |
| `ctest --test-dir Saved/PU20261009 -C Debug --output-on-failure` | 0 | `GamePlatformUI-Native.log`；NativePolicyOnly |
| `cmake --build Saved/PU20261009 --config Release` | 0 | `GamePlatformUI-Native.log`；NativePolicyOnly |
| `ctest --test-dir Saved/PU20261009 -C Release --output-on-failure` | 0 | `GamePlatformUI-Native.log`；NativePolicyOnly |
| `cmake -S Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Tests -B Saved/PV20261009` | 0 | `GamePlatformVFX-Native.log`；NativePolicyOnly |
| `cmake --build Saved/PV20261009 --config Debug` | 0 | `GamePlatformVFX-Native.log`；NativePolicyOnly |
| `ctest --test-dir Saved/PV20261009 -C Debug --output-on-failure` | 0 | `GamePlatformVFX-Native.log`；NativePolicyOnly |
| `cmake --build Saved/PV20261009 --config Release` | 0 | `GamePlatformVFX-Native.log`；NativePolicyOnly |
| `ctest --test-dir Saved/PV20261009 -C Release --output-on-failure` | 0 | `GamePlatformVFX-Native.log`；NativePolicyOnly |
| `pwsh -NoProfile -File Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Tests/Scripts/ValidateGamePlatformVFX.ps1` | 0 | `VFX-Static.log`；LocalStaticOnly |
| `pwsh -NoProfile -File Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Tests/Scripts/ValidateGamePlatformSurface.ps1 -WorkspaceRoot .` | 0 | `Surface-Static.log`；LocalStaticOnly |
| `pwsh -NoProfile -File Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Tests/Scripts/TestGamePlatformSFXArchitecture.ps1` | 0 | `SFX-Static.log`；LocalStaticOnly |
| `git diff --check -- Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation Game/Plugins/GamePlatform/Presentation/GamePlatformVFX Game/Plugins/GamePlatform/Presentation/GamePlatformSFX Game/Plugins/GamePlatform/Presentation/GamePlatformUI Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Docs Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/README.md Game/Plugins/MobaCommon/Presentation/MobaPresentation Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime Game/Plugins/DivineBeasts/DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md Game/Plugins/DivineBeasts/DBAClient/README.md` | 0 | `DiffCheck.log`；OwnedWhitespaceCheck |
| `python Saved/Validation/PluginRemediation-2026-10-09/Presentation/CollectSourceReview.py` | 0 | `SourceReview.json`；AuxiliaryLexicalReviewOnly |

首次四个过长构建目录配置均退出1，保存`*-Native-Attempt1.log`中的MSVC FileTracker FTK1011真实失败。缩短构建路径后同编译器/配置/测试通过；没有关闭检查。旧Attempt1 JSON只记录“cmake configure”，完整参数未保留。

F03首次生产评分测试实际红灯：`F03: scope must precede context specificity`，会话工具输出chunk `659b02`、复合工具退出1。当时未另存独立红灯转录，不能声称有不存在的日志；后续绿灯完整日志已保存。其余UE生命周期用例尚未执行，不把新增测试源码算红转绿。

## 未执行与资源缺口

- Editor/Client/Server统一UE5.8.0编译/UHT
- 所有Private/Tests UE Automation（包括本轮新增）
- 真实Definition/Niagara/EffectType/Sound/MPC/材质/Widget读取与运行
- AssetRegistry/DataValidation所有者/类型/Bundle/硬引用
- 跨世界Data多实例租约/旧回调/重入实际运行
- CommonUI覆盖/返回/初始化自闭/重入/GC/焦点
- MOBA迟到复制/重生/五模式/5v5/预测撤销/双LocalPlayer
- 干净Cook/Stage/服务器最终容器表现剥离
- GPU/CPU/音频/设备性能捕获
- Monolith资产生成/编译/保存/重载（本轮没有视觉资产任务）

F09和F15仍有真实内容缺口；F16归主执行者全局装配及最终容器验收。本组完成相应失败合同与证据记录，不宣称上述缺口已关闭。

## 中文人工说明与兼容迁移

已阅读本轮目录排序/预检、VFX定义与资源回退/Composite/预载/终态、SFX起播/取消/终态、Surface租约/稳定错误、UI离栈与代次重入、MOBA类型上下文/订阅、DBA内容事务/去重及七份中文伴随说明。

未声称全部存量公开字段/插件/Camera/内容文档已完成全量中文审核；根规划由主执行者同步。

全部63个触及源码有中文文件职责/端侧/所有权和正式伴随说明引用。辅助扫描仅检查文件头与分隔符；语义审核范围如上，不是全项目合规证据。

- 模块名、UObject稳定反射类型及资源挂载点未重命名。
- 旧Specificity保留序列化字段但不参与P13，工具目录需审六字段和Priority。
- 兼容VFX Catalog.Priority及Entry.Priority由百万权重改为64位和，旧配置复核。
- ContentPackHandle必须保存Id+ScopeId+Generation完整值，手写Id-only不再有效。
- Activate有效句柄只表示Loading受理，查询/事件Active才表示原子发布。
- RegisterProvider新增可选DefinitionClass默认空保留老入口；欲用于包预检必须登记真实类型；玩家无需反向引用执行类。
- Surface Pending枚举尾部新增，失败不被Unchanged覆盖；显式Refresh才换代重试。
- ViewModel OnPageEnded调用时旧代已失活；派生清理不能假设旧IsPageActive仍true。
- VFX/SFX Submitted/Queued只表示受理；GetPlaybackSnapshot/完成回调有界历史保留真实终态，RequestId/ParentHandle用于关联。
- 默认两个ID变为合法@1；不存在实际资产时保持缺配置且不发布默认目录。

## 完整文件清单与冻结证据

以下是本组范围内的最终交付文件。SHA256/字节数见`SourceFreezeManifest.json`及`RepairReport.json`；主执行者统一记录根目录新增清单和Surface描述依赖。共享仓库其他范围改动未撤销。

| 文件 | 变化 |
| --- | --- |
| `Game/Plugins/DivineBeasts/DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md` | added |
| `Game/Plugins/DivineBeasts/DBAClient/README.md` | modified |
| `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/DivineBeastsPresentationClient.Build.cs` | modified |
| `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/DivineBeastsPresentationClientSubsystem.cpp` | modified |
| `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Tests/DivineBeastsPresentationActivationTests.cpp` | added |
| `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Public/DivineBeastsPresentationClientSubsystem.h` | modified |
| `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/DivineBeastsPresentationRuntime.Build.cs` | modified |
| `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Catalog/DivineBeastsPresentationProjectCatalog.cpp` | modified |
| `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/ContentPacks/DivineBeastsPresentationContentPack.cpp` | modified |
| `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Tests/DivineBeastsPresentationRuntimeTests.cpp` | modified |
| `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Public/ContentPacks/DivineBeastsPresentationContentPack.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Docs/AuditRemediation-2026-10-09.md` | added |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/README.md` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/GamePlatformPresentationClientSubsystem.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/Tests/GamePlatformPresentationRegistryTests.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Public/GamePlatformPresentationClientSubsystem.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationCore/Private/GamePlatformPresentationCatalog.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationCore/Public/GamePlatformPresentationCatalog.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Docs/AuditRemediation-2026-10-09.md` | added |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/README.md` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Integration/Presentation/GamePlatformSFXPresentationBridgeSubsystem.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Tests/GamePlatformSFXLifecycleTests.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Public/Interfaces/IGamePlatformSFXService.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Public/Types/GamePlatformSFXTypes.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Docs/AuditRemediation-2026-10-09.md` | added |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/README.md` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/GamePlatformSurfaceClient.Build.cs` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Private/Subsystems/GamePlatformSurfaceWorldSubsystem.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Private/Subsystems/GamePlatformSurfaceWorldSubsystem.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Private/Tests/GamePlatformSurfaceBindingTests.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Private/Types/GamePlatformSurfaceTypes.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceClient/Public/Types/GamePlatformSurfaceTypes.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Docs/AuditRemediation-2026-10-09.md` | added |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/README.md` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Core/GamePlatformActivatableWidgetBase.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Manager/GamePlatformUIManagerSubsystem.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Screens/GamePlatformUIScreen.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Tests/GamePlatformUILifecycleRegressionTests.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Tests/GamePlatformUIReentryFixture.h` | added |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/Tests/GamePlatformUIReentryTests.cpp` | added |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Private/ViewModels/GamePlatformViewModelBase.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Public/Core/GamePlatformActivatableWidgetBase.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Public/Manager/GamePlatformUIManagerSubsystem.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Public/ViewModels/GamePlatformViewModelBase.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Docs/AuditRemediation-2026-10-09.md` | added |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/README.md` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Composite/GamePlatformVFXCompositeRunner.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Composite/GamePlatformVFXCompositeRunner.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Execution/GamePlatformVFXNiagaraExecutor.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Instances/GamePlatformVFXInstanceRegistry.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Instances/GamePlatformVFXInstanceRegistry.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Integration/Presentation/GamePlatformVFXPresentationBridgeSubsystem.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Integration/Presentation/GamePlatformVFXPresentationProvider.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Resolution/GamePlatformVFXCatalogRegistry.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Resolution/GamePlatformVFXCatalogScore.h` | added |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Tests/GamePlatformVFXCatalogScoreNativeTests.cpp` | added |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Tests/GamePlatformVFXDataIntegrationTests.cpp` | added |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Tests/GamePlatformVFXResolverTests.cpp` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Tests/GamePlatformVFXResourceOwnershipTests.cpp` | added |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Catalogs/GamePlatformVFXCatalogTypes.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Definitions/GamePlatformVFXDefinition.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Interfaces/GamePlatformVFXService.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Settings/GamePlatformVFXSettings.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Types/GamePlatformVFXPreloadHandle.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Types/GamePlatformVFXRequest.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Types/GamePlatformVFXResult.h` | modified |
| `Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Tests/CMakeLists.txt` | added |
| `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Docs/AuditRemediation-2026-10-09.md` | added |
| `Game/Plugins/MobaCommon/Presentation/MobaPresentation/README.md` | modified |
| `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationClient/Private/MobaPresentationClientSubsystem.cpp` | modified |
| `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationClient/Private/Tests/MobaPresentationPawnBindingTests.cpp` | added |
| `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationClient/Public/MobaPresentationClientSubsystem.h` | modified |
| `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationRuntime/Private/MobaPresentationRequestBuilder.cpp` | modified |
| `Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationRuntime/Private/Tests/MobaPresentationRuntimeTests.cpp` | modified |

全部新增路径均为ASCII英文目录/文件名，七份中文正式说明为Markdown内容。未重命名稳定反射/模块/资产身份。`Saved`证据为瞬态验证产物，不要求逐项写入正式目录规划。
