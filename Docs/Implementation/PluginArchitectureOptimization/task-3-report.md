# Task 3 实施报告：UI资源租约与异步根布局

## 状态与范围

- 状态：源码及回归用例已实现；轻量继承边界检查与最终差异检查通过。UE 编译、Automation 执行、真实资产回读、Cook/Stage 和视觉/联机验证待主代理统一执行，不能据此标记全部验收通过。
- 项目：`agent:freebooz-4b15fc0c7cf14418:divinebeastsworkspace-888d2615`。
- 原工作区：`E:\poject\feebooz\DivineBeastsWorkspace`；分支 `main`；启动时 HEAD `7d44194333cda24f8a14495695e1d478deef5a97`，保留其他任务的未提交工作。
- 需求依据：`task-3-brief.md`。主代理补充授权在同一 UI 范围内闭合已有 Description 缺失/空白校验回归。
- 未启动 UE 构建、Cook 或 Editor，未关闭任何编辑器，未提交/推送，未修改 Data 公开 API、UI 二进制资产、配置、其他模块、共享目录规划或执行台账。
- 源码及文档均通过 canonical `edit_project_files` 写入，现有文件使用新鲜 `read_files.read_revision`；本任务没有派生代理。

## 已确认的需求澄清

实际 `DefaultRootLayoutClass` 是 `DivineBeastsUIClientSubsystem.cpp` 匿名 namespace 内的 `const TSoftClassPtr`，不存在 brief 所述的“已有可配置反射软类字段”。主代理确认按真实源码保留该常量、稳定资源路径与反射类身份，不为误记新增 UPROPERTY 或配置来源。本次没有修改任何已有反射属性名称、资产挂载点或 PrimaryAsset 身份。

## 设计与实际接线

### 1. 技能栏 Profile 与图标各自持有中央普通资源租约

`UDivineBeastsAbilityUIProfile` 保留 `UPrimaryDataAsset` 基类和 `DivineBeastsAbilityUIProfile:HeroDefinitionId` 身份。ViewModel 使用 `UAssetManager::GetIfInitialized()->GetPrimaryAssetPath` 只读查询注册路径；加载和释放只调用现有 `IGamePlatformDataService::AcquireResources/ReleaseResources`，没有改用不适用的 `AcquireDefinition`。

第一阶段持有 Profile 租约，成功后从 `ProfilePath.ResolveObject()` 取得对象，检查英雄及 Profile 合法性。第二阶段对 Profile 图标软路径去重，批量取得独立图标租约。Profile 的 `AssetBundles="UI"` 作者元数据保留；运行时不再由此 ViewModel 调用 `LoadPrimaryAsset`，也没有独立 StreamableHandle 或全局 UnloadPrimaryAsset。

每份 ViewModel 资源需求都绑定当前 Loadout 所属 World/GI。Data 的弱调用者使用真实 Loadout，因为由 Widget 创建的普通 ViewModel 不一定自行提供 GetWorld；回调另外捕获并验证弱 ViewModel、原 Loadout、英雄、加载代次与完整签发身份。这样既满足 Data 的世界所有权合同，也不把 VM 存活与来源存活混为一谈。

解绑、重新绑定、换英雄、BeginDestroy 均先撤销代次和本地状态，再分别释放原图标/Profile 租约。释放使用捕获的原 GI，不重新从新英雄/新世界推断所有者。另一个 ViewModel 或本地玩家的租约不受影响。

Profile 未登记、加载失败或校验失败时保留安全缺省；失败终态租约显式释放。图标批次失败时保留已验证文本及其 Profile 租约，图标租约释放，`bIconsReady=false`。槽位只在本次图标租约成功且 Texture 已解析时发布软图标；纹理碰巧由其他调用者驻留也不能掩盖本次失败。GAS 授权、冷却、角色就绪和原有缺省文本语义保持原路径。

### 2. 默认根布局按 LocalPlayer 与控制器管理异步状态

根布局继续使用：

`/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout.WBP_DBA_UI_RootLayout_C`

请求弱调用者为所属 LocalPlayer，租约为 Instance 期限。显式状态为：

| 状态 | 含义与后续行为 |
|---|---|
| Idle | 等待有效本地玩家/控制器，尚未发起加载 |
| Pending | 一份在途普通资源请求；同上下文 Ensure 合并，不重复申请 |
| Failed | 加载、类型或安装失败；同一上下文的状态事件不无限重试 |
| Ready | 平台持有根实例；本组合根拥有的成功租约保留到替换/退出 |

初始化及真实控制器变化进入同一上下文协调逻辑。控制器撤销/更换会撤销旧 Pending；其他 LocalPlayer 的控制器被拒绝，不能误绑定 HUD/输入。已安装根仍属于同一 LocalPlayer 时，控制器 Travel 不重载该类，保留成功 Instance 租约。

完成回调核对代次、完整租约、当前 LocalPlayer、当前控制器、实际 World/GI、退出状态及 Data 的实时 Succeeded 状态。只用 `ResolveObject` 读取已加载 UClass，并校验根布局继承和非抽象类；未隐藏任何 `LoadSynchronous`。

外部合法安装先夺取旧请求资格。若旧根仍显示，旧成功租约保留到替换确实成功后释放；若是 Pending，则立即撤销本调用者旧请求。安装边界使用 `bInstallingRootLayout`，同步 Construct/平台回调不能再申请默认根或重入主页面同步。成功发布 Ready 后再调用 SyncPrimaryScreen；原有主路由同步保护继续保留。

Deinitialize 首先设置退出资格、撤销根布局代次并释放自身租约，再执行现有页面/适配器/委托清理；迟到完成不能重装布局。

### 3. 闭合已有 Profile 说明校验回归

实际 `ValidateProfile` 原先检查英雄、技能身份、唯一性、名称和图标路径，却没有检查 Description，因而已有两个回归会错误接受缺失或纯空白说明。

新增 `Description.ToString().TrimStartAndEnd().IsEmpty()` 拒绝及明确中文错误。保留两条既有测试的全部断言，仅更新已失真的“当前 RED”注释，并给旧基础验证夹具补充非空测试说明，使“缺图标/重复编号”继续各自验证其原有失败原因。未修改作者内容，也没有使用默认文案把不完整 Profile 变为合法。

已只读核对 `Tools/Unreal/Abilities/AuthorZodiacDevelopmentAbilities.py`：在 `update_ui_profiles` 分支，主动/非主动候选分别写入两种非空开发说明，然后设置 Profile 条目。此证据说明现行作者路径满足新合同，但不是已保存十二份资产的逐份引擎回读；实际资产值仍列入统一验证。没有取得“合法开发资源必然因本变更失效”的具体证据。

## 修改文件清单

所有模块路径均位于 `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/`。

| 文件 | 改动 |
|---|---|
| `Public/ViewModels/Combat/DivineBeastsAbilityBarViewModel.h` | 两阶段 Data 值租约、代次/弱 GI/状态及私有完成接口 |
| `Private/ViewModels/Combat/DivineBeastsAbilityBarViewModel.cpp` | Profile 查询、普通资源加载/释放、图标批次、过期完成和缺省接线 |
| `Public/DivineBeastsUIClientSubsystem.h` | 根布局四态、上下文/代次/租约及私有异步接口 |
| `Private/DivineBeastsUIClientSubsystem.cpp` | 异步根布局、控制器隔离、外部安装、重入保护与退出清理 |
| `DivineBeastsUIClient.Build.cs` | GamePlatformData 从 Private 移至 Public：公开 UI 头现持有其值租约合同，调用者必须能编译这些头 |
| `Public/Definitions/DivineBeastsAbilityUIProfile.h` | 保留全部反射字段/Bundle 元数据，更新资源与校验契约中文说明 |
| `Private/Definitions/DivineBeastsAbilityUIProfile.cpp` | Description 缺失/纯空白拒绝与文件职责说明 |
| `Private/Tests/DivineBeastsAbilityBarViewModelTests.cpp` | 保留既有断言，补充基础夹具说明，更新两条说明回归注释 |
| `Private/Tests/DivineBeastsUIResourceLeaseFixture.h`（新增） | 非抽象瞬态根布局类，只供本模块测试的软类/类型检查 |
| `Private/Tests/DivineBeastsUIResourceLeaseTests.cpp`（新增） | 六项直接驱动生产入口的行为回归 |
| `Game/Plugins/DivineBeasts/DBAClient/Docs/AbilityBarAutoBinding.md` | 更新技能栏租约、根布局状态、Description 合同及证据边界 |
| `Docs/Implementation/PluginArchitectureOptimization/task-3-report.md`（本文件） | 实施、命令结果、待验证及疑虑 |

未新增插件、模块、配置字段、反射资源身份或第二工程。新增两个测试文件需要由主代理在总体目录规划中登记。

## 完整 Automation 名称及覆盖

### 新增六项

| 完整测试名 | 直接验证的生产行为 |
|---|---|
| `DivineBeasts.UI.Resources.AbilityProfileAndIcons.HeroSwitchReleasesBoth` | 两 VM 独立租约；图标去重；真实 Loadout 广播换英雄释放自身两阶段资源；另一个消费者仍保留需求 |
| `DivineBeasts.UI.Resources.AbilityProfileAndIcons.StaleCompletionAndDestroy` | 旧英雄 Profile 迟到不发起图标；解绑后图标迟到不复活；重新绑定独立请求；ConditionalBeginDestroy 释放并解绑真实委托 |
| `DivineBeasts.UI.Resources.AbilityProfileAndIcons.MissingResourceFallback` | 缺 Profile、失败图标、未登记路径；失败终态释放；同英雄不循环重试；图标失败不借其他持有者驻留状态伪造成功 |
| `DivineBeasts.UI.Resources.RootLayout.PendingFailureAndControllerChange` | 真实 PlayerControllerChanged 入口、Pending 合并、Failed 锁定、换控制器新代次、过期成功、Deinitialize 后迟到完成 |
| `DivineBeasts.UI.Resources.RootLayout.ReadyExternalInstallAndReentry` | 成功资源完成只安装一次；ResolveObject 得到真实软类；安装重入保护；同玩家 Travel 保留租约；外部安装替换/抢占后的释放与迟到抑制 |
| `DivineBeasts.UI.Resources.RootLayout.LocalPlayerIsolationAndInvalidClass` | 两个本地玩家/实例互不释放；撤控制器/误投其他玩家控制器；已驻留非根 UClass 拒绝；外部空类拒绝 |

### 必须一并执行的既有回归

- `DivineBeasts.UI.AbilityProfile.Validation`
- `DivineBeasts.UI.Characters.HeroOverview.AbilityDescriptionRequired`
- `DivineBeasts.UI.Characters.HeroOverview.AbilityDescriptionWhitespaceRejected`
- `DivineBeasts.UI.Combat.AbilityBarReconstruct`
- `DivineBeasts.UI.Input.WorldHandoff`

前两条 Description 专项测试不是本轮新增；已按主代理授权修复其生产校验，不跳过/削弱断言。

### 测试替身边界

测试直接使用真实 ViewModel、Loadout 组件广播、UWorld/UGameInstance、LocalPlayer/Controller、项目 UI 子系统和平台 UI 对象。Data 替身只签发测试值租约、控制延后成功/失败与记录释放；它不重写消费方状态机，也不宣称测试了中央资源管理器自身。根布局成功用例只替换最终视口挂载边界，发布真实瞬态根对象，明确不是无窗口 Slate 挂载成功证据。

新测试的资源请求/回调均来自已接线生产入口：若仍走旧 LoadPrimaryAsset 或同步根加载，其 Data 请求计数及回调链断言会失败。没有以源码字符串断言代替生命周期行为测试。

夹具不伪造真实角色 Ready、服务端授权或已批准技能。缺资源用例证明租约/缺省状态和“不能凭资源发明授权槽位”，不替代真实已授权技能栏的文字、图标与冷却视觉验收。

## 实际执行的轻量验证

### 1. 差异检查

实际通过 Cwebcodex `run_script(language=powershell)` 执行：

```powershell
$taskScope = @(
  'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient',
  'Game/Plugins/DivineBeasts/DBAClient/Docs/AbilityBarAutoBinding.md'
)
git diff --check -- $taskScope
$taskExit = $LASTEXITCODE
Write-Output ('TASK3_GIT_DIFF_CHECK_EXIT=' + $taskExit)
```

一次复查发现接入文档末尾多余空行，输出 `AbilityBarAutoBinding.md:63: new blank line at EOF.`，退出码 2；已通过 revision-fenced 编辑删除该空行。

最终复查输出：

```text
TASK3_GIT_DIFF_CHECK_EXIT=0
```

同时 `git status --short -- $taskScope` 列出上述 9 个已跟踪修改文件及 2 个新测试文件，与本报告代码/接入文档清单一致。Git 的 LF→CRLF 工作区策略提示仍出现；没有差异错误。本检查不证明 C++ 编译通过，也不会把未跟踪测试文件当成已编译代码。

### 2. 三层继承与 Public API 源码边界

首次尝试 `run_process executable=pwsh -NoProfile -File Tests/Architecture/ValidateInheritanceBoundaries.ps1` 被工具在启动前拒绝：该 executable 未解析/不支持；`command_started=false`，不是门禁失败，不曾启动命令。

随后用已可用的 typed PowerShell 运行同一现有脚本：

```powershell
& (Join-Path (Get-Location) 'Tests/Architecture/ValidateInheritanceBoundaries.ps1')
```

实际输出：

```text
继承边界扫描：PublicHeaders=508; Types=1108; Edges=195
三层C++继承与Public API源码边界通过。此结果不替代真实Blueprint/DataAsset资产父类的UE AssetRegistry/DataValidation检查。
```

脚本正常完成，成功退出（脚本成功分支为 exit 0）。该结果为并行工作区当次扫描快照，不是 UE 资产父类或构建证明。

### 3. 锁定 UE 5.8 接口只读核对

使用 `rg -n` 检查主代理给出的 `F:\UnrealEngine-5.8.0-release\Engine\Source\Runtime\Engine\Classes\` 头文件，实际找到：

- `UAssetManager::GetIfInitialized()`
- `GetPrimaryAssetPath(const FPrimaryAssetId&) const`
- `UWorld::SetGameInstance(UGameInstance*)`
- `AActor::GetGameInstance() const`
- `APlayerController::GetLocalPlayer() const`

仅为精确接口核对，不替代 UHT/编译或 ABI 验证。

### 未执行

没有执行任何 UE Build、Automation、Cook、Stage、Monolith 资产操作、客户端画面、联机或多PIE运行。六项新测试及两项 Description 专项不能在此报告中记录为已运行或通过。

## 待主代理统一验证

1. 使用锁定 UE 5.8 编译包含 DivineBeastsUIClient 的 Editor 与相关 Client 目标；确认新增私有反射夹具 UHT、公开 Data 依赖及开发测试宏的编译。专用服务器仍不应装配本客户端模块。
2. 执行全部六项 `DivineBeasts.UI.Resources.` 测试，以及上列既有 Profile/说明/重建/输入回归；记录实际加载到的测试名、成功/失败数、命令、退出码和日志。
3. 用真实 AssetRegistry/Data 服务及已保存 Profile 做只读回读：十二份开发 Profile、条目 Description 非空白、两阶段图标租约和实际缺失资源路径。现行作者脚本的字段赋值不能替代此证据。
4. 真实本地玩家旅行、双本地玩家/多PIE、控制器撤销/替换、外部根安装和注销后检查：没有重复根请求/重装、没有另一玩家资源丢失、无旧英雄图标回写，缺资源保留既有中性视觉与真实 GAS 资格。
5. 按统一计划执行 Cook/Stage 及客户端视觉/服务端剥离检查。代码改为软路径普通资源租约后，现有 Bundle 作者元数据和 Cook 扫描保持原样，但需真实产物确认覆盖。

## 疑虑、边界与中文说明审查

- 原根布局常量与 brief 的可配置字段描述不一致，已由主代理确认按实际源码保留，未扩大配置范围。
- Description 校验有意收紧已有“说明必需”合同。现行作者脚本填充非空开发说明；已保存资产尚未在本任务读取。若旧 Profile 缺说明，它将走现有安全缺省，不会被自动补文案或改 GAS；需要资产回读明确受影响项。
- 根布局同上下文失败后不自动重试，这是避免事件风暴的明确策略；恢复入口为真实控制器新上下文或合法外部安装。没有加入计时重试、Tick 或隐藏同步兜底。
- 图标批次有任一资源失败时，本 VM 的该批次按 Data 合同失败，保留 Profile 文本并不发布任何“本次已就绪”图标；不冒充逐图标部分加载成功。
- 六项生命周期回归使用明示边界替身，不证明真实 StreamableManager、多实例中央聚合、Viewport/Slate、生产资产、网络权威或显存释放。相应证据由平台既有测试和统一 UE 验证补足。
- 新/改生产路径与测试夹具均补充中文职责、线程、租约所有权、取消、代次、失败和测试替身边界说明。旧说明测试的失真 RED 注释已同步更新；未对无关存量模块作“全部中文合规”声明。
