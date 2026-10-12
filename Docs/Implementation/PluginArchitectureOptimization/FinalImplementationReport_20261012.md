# 插件架构优化实施报告（2026-10-12）

## 1. 执行结论与最佳方案

本轮选择保留现有三层插件划分及稳定身份，在原主工程内修复已经证实的资源所有权、生命周期、依赖暴露、平台路径耦合与验收证据问题。已实施六项整改，真实构建、Automation、资产回读与打包结果分别记录。本报告中的“已实现”指源码已经落地；“通过”只用于所列实际执行及其限定范围，不表示完整业务或正式发行已经完成。

这一方案适合当前项目的理由有三点。第一，结构审计确认当前64个插件描述、47个代码插件、17个内容包和三目标声明装配成立，全面改换插件身份和资源路径会增加迁移工作。第二，实际问题集中在跨插件所有权、依赖可见性和证据消费方式，能够在既有职责边界内修复。第三，现有模块成熟度存在差异，应依据消费者和可验证能力决定后续保留或退休，而不以目录数量推断功能完成。

当前结果是：六项源码整改与开发Probe/Flow资产工作已落地；四组实际UE报告合计95条Success、1条既有UI失败，33个指定测试身份全部通过。正常Server暴露共享引擎旧HTTP测试的编译错误，新的正常Client及实际Cook尚未完成，不能标记为全部验收通过。

本轮继续由 GamePlatformData 统一承担资产身份、加载请求合并和资源租约；平台层保留项目无关能力，项目默认值由项目工具提供；内部后端实现和客户端表现资源在各自边界管理。没有新增第二个主工程、另立资源管理器，或将开发内容自动改为正式批准。

## 2. 实施计划与落地结果

| 次序 | 任务 | 原问题 | 实施结果 | 本轮验收方式 |
|---|---|---|---|---|
| 1 | 服务器资源规则统一 | 不同服务器配置维护分散，表现资源排除存在漏项，配置正确不能证明产物正确 | 建立规则JSON单源、Schema、确定性配置生成与漂移检查；接入Server Cook前检查及Cook后真实容器禁入审计 | 25项规则回归、真实UBT配置解析通过；实际Cook因正常Server前置构建失败而未执行 |
| 2 | 定义唯一性索引 | 唯一源查询反复扫描，资产变更与退出需要保持一致性 | 私有双向索引、一次冷建、按注册表事件增量维护；保留证据不足时的拒绝行为、候选覆盖与解绑 | Data16项真实UE回归，含6项新增索引/Registry测试 |
| 3 | UI租约与异步根布局 | Profile/图标资源所有权分散，同步根加载及迟到回调存在生命周期风险 | 通过Data分别持有Profile和图标租约；根布局使用Idle/Pending/Failed/Ready状态及上下文代次；补齐Description验证 | 11项指定UI回归、完整UI前缀、12份Profile/60项条目与图标真实回读 |
| 4 | Arena接口与VFX诊断 | 服务器内部实现暴露为公共依赖，多世界诊断相互干扰 | 内部头转Private，HTTP/JSON依赖收口；按世界注册/更新/注销计数，集中处理Pending变化 | 公共/竞技声明装配、Editor目标内Server模块编译及VFX24项UE回归；独立Server目标未通过 |
| 5 | 平台路径和交付登记 | 平台Gold作者含项目默认根，内容包职责和模块成熟度缺少统一记录 | BlueprintRoot显式参数化；项目入口提供默认；补齐13包README及64插件/85模块清单，8个保留模块不计作生产能力 | PCG3项参数回归、原并行改动保护、结构及清单验证、正式内容门禁 |
| 6 | 独立审查与统一验证 | 零测试、模糊名称、过期DLL或仅路径存在容易被误判为成功 | 完整Editor构建凭据、源输入/产物前后校验、精确测试身份、额外失败拒绝及独占RunId证据目录 | 脚本合同/反例回归、完整目标构建、真实UE执行、独立复核及最终清单 |

### 2.1 服务器规则

服务器纯表现目录由同一份规则定义，受管段覆盖 DedicatedServer、VillageServer、VillageDevelopmentServer 的 Game 配置及 DedicatedServer 的 Pak 配置。在本次固定v快照中，FoundationStandalone仍保留开发地图和定义许可，Server调用侧追加128个独立数组参数；客户端不会因此获得服务器排除配置。原工作区后续Terrain规则扩展另见第3节，数量不与固定快照混用。

规则校验保留权威定义、地图、碰撞和导航边界，拒绝未知字段、路径穿越、错误挂载、重复规则、配置漂移以及受管段之外对同一数组的覆盖。真实容器审计枚举本次Stage，对每个Pak/IoStore重新取证，检查空或错误清单、必需容器、指定地图及表现目录泄漏。

### 2.2 Data索引

索引保存 LogicalId 到物理资源路径集合及反向映射，不持有定义UObject、世界或资源租约。模块启动只绑定事件；资产发现完成后的首次查询执行冷建，暖查询直接查对应身份的路径集合。资产增删、重命名、身份变化按规则增量更新；发现重新开始或类资格相关事件使旧快照失效后，由下一次就绪查询重建。

唯一源解析继续拒绝非法查询身份、未就绪、缺失与重复来源。编辑器候选仅替换本次查询中的自身来源，不修改共享索引；AssetManager的AddDemand独立核对主资产映射与唯一源路径，不一致时返回PrimaryAssetMappingMismatch。构建快照期间发生事件或Shutdown会使旧代次不能发布；模块退出解除全部八个注册表委托。已验证重复查询不会再次全量枚举；没有将这一行为结果换算为未经测量的耗时或内存改善百分比。

### 2.3 UI资源与根布局

能力栏通过Data的World期限普通资源租约分别持有Profile和图标。回调核对弱ViewModel、原Loadout、英雄、代次及完整签发身份，并向原GI的Data服务确认租约仍为Succeeded；Data层负责租约所属实例和World生命周期检查。解绑、换英雄或销毁时仅释放自身两份需求。图标批次失败时保留已验证文本，图标置空。

根布局异步请求按LocalPlayer与控制器管理。同一上下文的Pending请求合并；Failed状态不因普通事件无限重试；退出或控制器变化使旧完成失效；合法外部安装与同步回调重入有明确交接。保持原有资源路径和反射身份，未为早期需求文档中误记的“可配置字段”额外扩展接口。

Description校验拒绝缺失或纯空白内容。真实资产回读确认12份开发Profile、60项技能身份/名称/说明和图标满足当前合同，未据此认定正式业务内容获批。

新增六项UI Automation使用实际消费方、World、LocalPlayer和Controller，但Data完成事件及最终根布局挂载采用受控测试边界；真实资产回读另行取证，多玩家实际视口安装和旅行场景仍待后续验证。

### 2.4 Arena与VFX

Arena的BackendClient和Coordinator内部头移入Private，保留公共项目扩展契约。实际Editor16收据包含GamePlatformArenaServer与DivineBeastsArenaServer等模块；AdditionalPlugins为空不等于这些模块未装配，因为三种Target本身启用DBAArena。

VFX按世界记录实例计数，关闭一个世界不会清零其他世界，迟到更新不会重新注册已退出世界。全局峰值表示模块生命期内同时在场数量峰值，累加使用宽类型并处理溢出。十条Pending变化路径收敛到即时发布入口。鸟群测试使用真实瞬态GameWorld及配对WorldContext，继续执行原Actor/ISM断言；这不构成BeginPlay、人工画面或目标设备性能验收。

### 2.5 PCG和成熟度

Gold的BlueprintRoot在Map/RepairMap副作用之前校验，平台层没有项目默认挂载点，项目工具层提供现有默认值。保留既有并行PCG的空间探针、生成和确定性工作，不把保护或兼容这些改动记成本轮新增功能。

成熟度清单明确分开SourcePresent与Reserved。64个插件和85个模块是登记事实，8个保留模块仍须有实际消费者、独立职责和生命周期证据才能升级为生产能力。

### 2.6 验收入口与发现后的修补

统一UE入口要求完整成功Editor构建，逐项核对该构建的前后输入清单、真实receipt、项目DLL/.modules和引擎工具身份；本次Editor16的输入清单为2533项。OnlyModules结果不具备完整Editor测试资格；精确测试名称必须唯一且Success，前缀内额外失败同样阻断。38项脚本合同回归、独立复跑和13项针对性反例是明确的离线验证，不能替代UE或UBT。

真实执行还发现并处理了几个限定问题：RootLayout夹具需要将控制器真实登记进World；UI颜色测试需要真实RHI，本轮使用D3D12离屏执行；空角色选择、取消入口和拖拽方向按照既有合同修正；鸟群夹具补齐实际GameWorld/WorldContext；开发定义作者修复execution_mode传递及EditDefaultsOnly节点的构造方式。旧失败、中止和过期构建记录均保留。

## 3. 验证对象与证据边界

正式工作区为 `E:/poject/feebooz/DivineBeastsWorkspace`；运行验证检出为同一授权根内的 `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v`。起始HEAD为 `7d44194333cda24f8a14495695e1d478deef5a97`，验证检出的Git基点为 `f2cf1b6fcdcaa68ecb9c96e7cabef62fa4d0d740`。本轮没有执行提交、推送或全树重置；外部提交和并行改动均保留。

原工作区曾取得完整Editor成功结果，包括 `b813c0ed-9d98-47a5-94b8-20c3a3684458`，但后续PluginDemo增量使新的源码门禁失效。因此后续验证固定在v的Editor16，而不继续将持续变化的原目录与旧DLL混用。原工作区后续未纳入该快照的功能仍需其自身验证。

最终Editor16：

- RunId：`20261012-0400-4000-8000-000000000016`。
- 实际结果：Passed / Eligible，退出0，完整增量Editor目标。
- 输入数：2533；输入SHA256：`E1AE7CD47BEFFC102C369264E98405F5CD08D96E6999B978CCCF7B85CAE6F67A`。
- 构建结果文件SHA256：`99A1E0A33ABC0688DD9F4758406BABDFF3BC9E52BC5AF36CA9DDC58C32D5F914`。
- 项目产物聚合SHA256：`E2D8E29F81D7B0BFD5D6D99B903E4D53BC15C4FA58B145C3E7EB6733394DD237`。

该构建使用UsePrecompiled复用现有引擎依赖，项目Editor目标实际编译/链接成功；它不证明本轮重建全部引擎源码，也不要求每个未变化的DLL都重链。正常Server已实际启动并暴露共享引擎HTTP测试漂移；新的正常Client未执行，不能据Editor16推定其通过。

Data11与PCG8真实绑定Build5，VFX18与UI20真实绑定Build16。07:01:59 UTC的实际比对确认，Build5→16的Data42项、PCG82项输入全部不变，四个对应Editor DLL的保存哈希及当前文件字节全部相同。该比对支持保留此前有效回归，没有重新运行测试或改写其绑定。

07:23 UTC对61个重点路径的只读比对发现：38项字节一致，10项仅CRLF/LF差异，11项存在内容差异，2项为预期移入Private的旧Public路径缺失。Data与Arena/VFX重点源码没有额外内容漂移；UI子系统差异来自original并行Demo接入，服务器配置差异来自并行Terrain扩展（CookDirectories为original 46 / v 45，StageOnly均20）。这些差异不并入本轮固定编译快照。

这次比对还发现v的Task5项目脚本未带齐原工作区已有的BlueprintRoot接线。随后仅恢复v的RunGoldLevelAuthoring.ps1、VerifyPCGGoldAuthoring.py及README中对应增量，实际PowerShell AST从RED退出1转为GREEN退出0，静态检查和两个原生命令参数接线均通过。该次b09252ef-b75d-449e-a1a2-b3eae0577a88核验Editor16原有2533个输入路径仍全部字节一致；三份文件位于编译输入根之外，没有新运行UE、重绑PCG8或重新发现输入清单。original三份文件未变，已有空间探针等并行增量继续保留。

输入端点清单覆盖项目编译相关目录、描述和配置；它不覆盖资产内容、引擎源码或SDK重建。具体纳入范围由FoundationProjectCompileInputs-v2记录，后续生成文件如落入已纳入目录，也会触发原有输入门禁。UI20使用真实D3D12离屏渲染，并记录其子环境跳过全平台SDK发现；其失败也使runner未执行后置proof。开发定义作者中的同类开关仅作用于该次Editor子进程，明确记录SDK未验证。正常构建与Cook不使用这一开关。

## 4. 实际验证矩阵

| 检查 | 实际结果 | 证明的范围 |
|---|---|---|
| 固定v结构审计 | Passed，exit0；64描述，47代码/17内容，三目标公共/竞技声明装配全部成立 | 结构、登记和继承边界 |
| 完整Editor16 | Passed / Eligible，exit0 | 固定快照的完整增量Editor目标及本地源码/产物一致性 |
| Data11 | 16/16 Success；exit0；后置proof通过 | 定义身份、租约与6项新索引/Registry回归；绑定Build5 |
| UI20 | 52/53 Success、1 Fail；UE退出0、runner退出1 | 11项指定架构回归通过；完整UI前缀仍失败；绑定Build16 |
| VFX18 | 24/24 Success；exit0；后置proof通过 | 三项诊断及同前缀受影响回归；绑定Build16 |
| PCG8 | 3/3 Success；exit0；后置proof通过 | BlueprintRoot规范、无效输入和阶段前置；绑定Build5 |
| 开发UI资产只读回读 | 12/12 Profile、60/60条目/显示名/非空说明及图标通过 | 完整Editor/Monolith中的真实开发资产；非视觉或正式批准 |
| Server规则 | 25项Python回归通过；完整UBT配置解析通过 | 单源、漂移、数组合并和容器证据解析合同 |
| 完整Editor证据合同 | 38项通过、独立38项复跑通过、13项针对性反例符合预期 | 离线脚本合同，未以合成产物替代真实构建 |
| 开发定义作者离线回归 | execution_mode修复53/53；节点构造修复original53/53、v现有50/50 | 明确的离线替身/语义回归，真实UE结果另列 |
| 正式内容CLI | inventory退出0；release退出2，Blocked | 现有正式登记的磁盘预检及前置门禁 |

四组UE报告合计96条测试记录，其中91条普通Success、4条带警告的Success、1条Fail；它们来自上述各自的实际RunId，而非一次新的统一执行。入口要求的33个具体测试身份在各自报告均为唯一Success；完整前缀仍会因额外失败被拒绝。

### 唯一未闭合的UI失败

`DivineBeasts.UI.Characters.HeroOverview.HeroIntroductionDataAssets` 在反射检查阶段发现HeroDefinition缺少可本地化的原生FText `Introduction` 字段。该用例与起始基线一致；本轮尚未进入其十二份英雄介绍数据检查，不能将当前失败仅归因于“缺内容批准”。

该合同还要求空CDO默认值和十二份已保存资产分别具有非空介绍。只增加字段或给出通用默认文案都不能完成它。本轮Task3范围是资源租约、异步根布局及限定追加的Description校验，因此保留这一既有失败；完整UI交付需将字段、作者入口、逐英雄内容和真实复验一起完成。

四条带警告的Success分别来自Data.ResourceLeases（测试缺失包）、UI.PlayerStatusSnapshot（GameplayCue扫描路径）、VFX.Attachment.WeakAndWorld（瞬态WorldContext）和VFX.Data.AbstractConstraintMissingDefinitionAndNaturalLeaseRelease（定义加载失败）。其实际JSON状态均为Success，仍保留警告原文；没有将它们隐去或自动改为失败，也没有据此修改其他业务配置。

## 5. 开发夹具、Server/Client与实际Cook

### 5.1 真实Foundation开发定义

RunId `f9f74a78-5044-47a5-916f-18368b6f0656` 的真实UE作者入口已Passed、整体exit0。Probe与Flow两个UE进程各自退出0；前后完整Editor16凭据核验通过，2533项编译输入和项目产品保持对应，`BuildEvidenceRecheckedAfterCreation=true`。结果文件SHA256为 `BD4A67EE3CE6C34D9738D05C5937F22076DD0464C6527A4FCA5D0AE5AA5DAC67`。

| 原生开发包 | 实际结果 | 文件字节与SHA256 |
|---|---|---|
| `/Game/Development/Foundation/Definitions/DA_FoundationProbe` | VALIDATED；只读校验，未调用保存或修复 | 1834 bytes；`0AA625E99A832DB4CA3D6AB2CB3A6CAF438AD85AD85B697A612D8232277686D5` |
| `/Game/Development/Foundation/Definitions/DA_FoundationFlow` | CREATED；保存、非空包、重载及作者字段/标签合同通过 | 4516 bytes；`36AB5F7680914334EAC18CF7C9247C800712BB36D2B2F767195DEBD3844E57B9` |

完整原生资产审计确认本次仅新增Flow，Probe无字节变化。随后在新的传输证据 `332418fd-b826-49f7-9a3e-856e5a198b96` 中，重新检查original目标及同名边车均不存在、两端相关十份类/身份源码仍与先前记录一致，再以`overwrite=false`传回Flow。传输后两端均为上述4516 bytes和同一SHA，original Probe保持原字节；传输验收Passed/exit0。

实际UE执行与原生重载发生在v；original只接收相同字节，没有冒称在original重新运行了UE。子进程显式记录 `UE_SKIP_UBT_SDK_SETUP=1`，仅用于该次已编译Editor开发资产作者；`SDKValidationPerformed=false`。这不是正式内容批准、SDK验收、Cook或Flow业务运行验收。

Probe/Flow的原生类、共同基类、身份与序列化相关十份源码在original和v定向核验为逐字节一致（b5396f87-c5f3-4041-8ffb-5819e3eda596）。此项支持限定传回检查，不等同两个工作区全部DLL或资产兼容性证明；最终资产身份以实际UE保存、重载和AssetRegistry标签报告为准。

### 5.2 正常Server构建的实际阻塞

本轮在v实际执行正常完整Server构建：RunId `4f4fb9ee-acab-4a74-98dd-e4681068a478`，不传UsePrecompiled、OnlyModules或SDK发现跳过，启用正常共享PCH并将并发限制为2。UBT使用VS 14.44和Windows SDK 10.0.22621.0，动作计划共1111项。

实际 `[503/1111] Compile Module.HTTP.cpp` 的动作退出2，UBT明确记录本次构建将失败。错误位于共享引擎现有测试文件 `F:/UnrealEngine-5.8.0-release/Engine/Source/Runtime/Online/HTTP/Private/Tests/HttpRedirectPolicyTests.cpp`：

- 第18行从派生测试访问Curl类私有CleanupRequest，产生C2248。
- 第21、154行使用不存在的 `EAutomationTestFlags::ApplicationContextMask`，分别产生C2838和C2065；实际引擎头提供的是 `EAutomationTestFlags_ApplicationContextMask`。

这是项目已有HTTP定制补丁的测试版本漂移。三份生产HTTP源码已经与项目登记的AfterSHA一致；仅测试源码仍处于旧版本。已经存在于项目补丁中的修正是在测试fixture中加入 `using FHttpRequestCommon::CleanupRequest;`，并修正两处上下文标志。经由protected基类虚接口调用，无需将Curl实现的私有成员公开。

确认不可成功后，07:48 UTC只停止本次自有Job `wc_job_2Yuz3OEh7aEmYk5c`，真实整体终态为Stopped / Cancelled、exit -1；原生日志中的HTTP编译失败与这一运行终态分别保留。07:48:53 UTC确认已知自有wrapper、UBT及直接子进程结束，`Build/result.json`不存在，未生成成功Server收据。另一项Surface编辑器任务自然结束，本任务未停止它。

### 5.3 共享引擎修复的具体交付

已准备三个可版本管理的审查文件，位于 `E:/poject/feebooz/DivineBeastsWorkspace/Docs/Implementation/PluginArchitectureOptimization/`：

- `SharedEngineHttpTestRepairProposal_20261012.md`：原始诊断、精确范围、共享影响、执行与回退步骤。
- `SharedEngineHttpTestRepairProposal_20261012.patch`：只包含一行基类using声明与两处上下文标志修订。
- `SharedEngineHttpTestRepair_20261012.ps1`：默认只读PlanOnly，只有显式Execute才进入已记录字节的修复事务。

精确修复前测试SHA为 `26E39B9061D0526B47DA802CB6763EF09434183AC5C06278E107C8310FA1C672`，修复后必须为既有登记值 `2861D419369AA45D62AC9FE8AC9823A9CCF706A7D9ED9B204E51F6C6AF26225B`。

此外，仓库现有完整补丁保存为347个CRLF，原字节SHA为A49EAD79…；规范为LF后恰好匹配原登记的8F78CDD1…。因此恢复分两步：先恢复补丁材料为既定LF字节，再同步该已知旧测试。恢复入口锁定两个文件的准确前后SHA及三份生产HTTP文件、原applier和hash清单的不可变摘要，执行前备份，写入时独占文件句柄，失败时只回退仍匹配本次写入版本的文件；发现其他变更即保留并拒绝覆盖。现有applier及期望摘要不作修改。

在Saved隔离副本中，真实演练了原材料摘要拒绝、仅恢复LF后旧测试拒绝、正式入口只读计划、精确执行、原样applier的CheckOnly返回AlreadyApplied，以及恢复原始两份文件字节。另在真实original与共享引擎上执行了最终入口的只读PlanOnly，返回TargetsWritten=false、ExistingGateChanged=false和预期四个摘要。真实共享引擎未执行修复，原应用器未在真实引擎执行CheckOnly；副本演练不属于C++、HTTP网络或UE回归。

下一步须按仓库 `AGENTS.md` 关于高影响变更的具体授权与回退要求，确认共享引擎同步范围并安排无其他使用者的执行窗口。可批准动作已经限定为“恢复这一份补丁材料的LF字节、同步这一份引擎测试文件的三处修订，再重建和运行列出的实际验收”；不是重新设计或重放整个HTTP补丁。

现有生产HTTP补丁增加了虚接口；Editor16复用现有引擎依赖，因此不能证明这些引擎源码与HTTP DLL及全部调用方已经一起重建。Data/UI/VFX/PCG的实际运行结果仍保留各自限定范围，HTTP运行及后端网络行为尚无本轮完整证明。

### 5.4 Client与Cook的状态

新的正常v Client构建没有执行。读取original既有正常Client生成输入可见 `WITH_DEV_AUTOMATION_TESTS=1`、`WITH_CURL=1`，且同一HTTP测试文件被纳入编译；当前v Server也满足同一门控。由这些实际输入判断，正常Client重编同一HTTP单元仍会碰到相同前置错误；这是有依据的推断，不记录为一次实际Client失败。

本轮Server Cook/Stage没有执行，交付清单设为Blocked、RunId保持null。必须先取得正常Server成功产物与有效Editor凭据，再运行选定FoundationStandalone配置的实际Cook，核对容器、两张Foundation地图及权威Probe/Flow定义。25项策略回归、UBT数组合并、原生定义存在和工具退出0都不能代替这一结果。


## 6. 仍需独立完成的完整场景

| 场景 | 当前结论 | 完成所需工作 |
|---|---|---|
| UI完整前缀与产品界面 | 一项既有Introduction合同失败；11项本轮指定架构回归通过 | 补齐原生字段与作者链、逐英雄内容并真实重验；客户端旅行、多本地玩家和画面验收分别执行 |
| VFX统一内容审查 | Blocked；original与v均无规定的L_VFXReview包 | 按10种Behavior和17类ContentCategory选择代表组合，形成真实审查地图、人工画面与目标设备性能证据 |
| PCG Gold完整场景 | NotRun；参数合同已通过 | 继续既有PCG任务的真实生成、SpatialProbe、Spawner与双进程确定性验证；开发Gold不因缺正式Gaea内容而一并阻断 |
| 正式竞技五模式与后端 | Blocked | 明确地图/胜利规则/五模式配置，完成Assignment、Ticket、Result实际后端联调 |
| 正式技能与内容交付 | Blocked；release实际退出2 | 完成正式身份、中文名称、数值和默认授权批准，再建立正式Profile、权威授权和端侧交付证据 |
| 其他服务器配置与性能 | 按各自实际证据登记 | 一次FoundationStandalone打包不证明VillageServer、VillageDevelopmentServer或所有设备已实际验证 |

现有正式技能清单的实际release命令报告70条前置消息，正式AbilityId和中文技能名称批准均为0，十二个默认AbilitySet均未配置。该清单日期仍为2026-10-09；70条消息是现有登记的前置输出，不是本轮新增70个UE失败。开发清单中的60个定义、12个Profile，以及本轮真实回读结果，继续保持开发范围。

## 7. 后续推进顺序

首先处理已经实际阻断正常目标构建的共享引擎HTTP测试文件漂移：按本报告附属的三处差异与旧/新SHA取得具体共享引擎变更授权，保持哈希门禁和可回退原文件，在无其他使用者时同步。由于此前生产HTTP补丁包含虚表接口变化，随后应完成受影响HTTP与调用方的正常重建、实际HTTP回归，再建立新的完整Editor凭据并继续正常Server、Client及有条件Cook；不能继续依靠UsePrecompiled把旧HTTP二进制视为已验证。

随后审阅本轮固定快照；原工作区并行Demo等增量结束后，建立其自己的完整构建证据。将HeroIntroduction作为字段、作者链、逐英雄内容和真实复验的完整工作项闭合；现有PCG任务继续Gold空间/确定性验证，VFX任务建立统一审查地图与设备预算。正式竞技与技能发行在相应业务输入和后端条件具备后分别验收。

维护时继续使用单源服务器规则、精确Automation身份和独占证据目录。索引、租约及多世界计数今后的性能收益应通过真实场景测量；不在没有基准数据时写入比例收益。保留模块只有在职责、消费者和生命周期证据齐备后才升级成熟度。

## 8. 证据与复现入口

### 8.1 实施文件

计划、执行台账和交付清单位于：

- `E:/poject/feebooz/DivineBeastsWorkspace/Docs/Implementation/PluginArchitectureOptimization/ImplementationPlan_20261012.md`
- `E:/poject/feebooz/DivineBeastsWorkspace/Docs/Implementation/PluginArchitectureOptimization/ExecutionProgress.md`
- `E:/poject/feebooz/DivineBeastsWorkspace/Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json`

同目录的task-1-report、task-2-report、task-3-report、task-4-report、task-5-pcg-report、task-6-entry-review和task-6-build-evidence-report保留各阶段源文件清单及独立审查。它们的交接时状态属于历史记录，当前结论以本报告与执行台账中明确对应的运行结果为准。

### 8.2 实际证据入口

| 证据 | 结果文件绝对路径 | 阅读限定 |
|---|---|---|
| 完整Editor5 | `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Saved/Validation/FoundationM0/20261012-0400-4000-8000-000000000005/Build/result.json` | Data11和PCG8的实际绑定 |
| 完整Editor16 | `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Saved/Validation/FoundationM0/20261012-0400-4000-8000-000000000016/Build/result.json` | VFX18和UI20的实际绑定 |
| Data11 | `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Saved/Validation/FoundationM0/20261012-0400-4000-8000-000000000011/PluginArchitectureTests/result.json` | Data/Automation/index.json保存16项逐项状态 |
| UI20 | `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Saved/Validation/FoundationM0/20261012-0400-4000-8000-000000000020/PluginArchitectureTests/result.json` | UI/Automation/index.json保存52 Success/1 Fail，另见UIRun20/Invocation.json |
| VFX18 | `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Saved/Validation/FoundationM0/20261012-0400-4000-8000-000000000018/PluginArchitectureTests/result.json` | VFX/Automation/index.json保存24项逐项状态 |
| PCG8 | `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Saved/Validation/FoundationM0/20261012-0400-4000-8000-000000000008/PluginArchitectureTests/result.json` | PCG/Automation/index.json仅为3项参数测试 |
| 结构审计 | `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Saved/Validation/PluginArchitectureOptimization/20261012/ArchitectureScenario/ca0faf13-ff77-471f-b9e2-956a44627e27/result.json` | 实际PS7静态执行 |
| Data/PCG字节对应 | `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Saved/Validation/PluginArchitectureOptimization/20261012/ModuleEvidenceCorrespondence/bcd70aca-1760-4730-ad0b-3320ebbadc3e/result.json` | 不重绑历史测试 |
| 开发UI资产回读归档 | `E:/poject/feebooz/DivineBeastsWorkspace/Saved/Validation/PluginArchitectureOptimization/20261012/UIProfileReadback/Run-20261012T051047Z-12c2246d/result.json` | 实际执行工作区为v |
| Flow作者离线修复 | `E:/poject/feebooz/DivineBeastsWorkspace/Saved/Validation/PluginArchitectureOptimization/20261012/FoundationFlowConstructorFix/b4fd2d67-340a-44e4-ab7f-ce54fa6ee921/report.md` | 原生失败、RED/GREEN及前后字节 |
| 正式内容门禁 | `E:/poject/feebooz/DivineBeastsWorkspace/Saved/Validation/PluginArchitectureOptimization/20261012/ContentDeliveryFinal/3356ef8e-18de-4b39-acec-0ac9d5f4045e/result.json` | Inventory/result.json、Release/result.json和原始stdout |
| 关键整改差异快照 | `E:/poject/feebooz/DivineBeastsWorkspace/Saved/Validation/PluginArchitectureOptimization/20261012/KeyImplementationComparison/ae658598-ca88-4f5a-933f-2f6c7e033c82/result.json` | 61路径定向比对；PCG脚本随后限定补齐另记新证据 |
| v项目PCG接线补齐 | `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Saved/Validation/PluginArchitectureOptimization/20261012/Task5VEntryRestore/b09252ef-b75d-449e-a1a2-b3eae0577a88/result.json` | 3文件最小恢复、AST RED/GREEN及原有2533输入保持 |
| 原生类定向兼容 | `E:/poject/feebooz/DivineBeastsWorkspace/Saved/Validation/PluginArchitectureOptimization/20261012/FoundationDefinitionCompatibility/b5396f87-c5f3-4041-8ffb-5819e3eda596/result.json` | 10源文件的时点比较，未执行UE |
| Probe/Flow真实作者 | `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Saved/Validation/FoundationM0/f9f74a78-5044-47a5-916f-18368b6f0656/FoundationDefinitions/result.json` | 两次原生UE均exit0、完整Editor16前后proof通过 |
| Flow无覆盖传回 | `E:/poject/feebooz/DivineBeastsWorkspace/Saved/Validation/PluginArchitectureOptimization/20261012/FoundationDefinitionTransferFinal/332418fd-b826-49f7-9a3e-856e5a198b96/result.json` | 真实4516字节、目标不存在前置及20次源码身份比较；无original UE执行 |
| 正常Server失败日志 | `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Saved/Validation/FoundationM0/4f4fb9ee-acab-4a74-98dd-e4681068a478/Build/Server/UBT.log` | 5条HTTP编译诊断；整体Job取消另在修复证据记录，无Build/result.json |
| 共享引擎修复准备 | `E:/poject/feebooz/DivineBeastsWorkspace/Saved/Validation/PluginArchitectureOptimization/20261012/SharedEngineHttpTestRepair/b34b20fc-6554-464e-b585-dbffec8e9123/result.json` | 实际字节捕获、隔离应用与回退演练；真实共享引擎未修改 |
| 交付静态复核 | `E:/poject/feebooz/DivineBeastsWorkspace/Saved/Validation/PluginArchitectureOptimization/20261012/FinalDeliveryChecks/77fd4f80-066c-48a0-b28c-4998663bf74b/result.json` | 清单合同、差异与交付文件状态的实际结果，未重新执行UE/UBT/Cook |

### 8.3 复现原则与命令

以下命令是共享引擎补丁状态核对、授权同步和所需HTTP/调用方重建完成后的复验模板。实际历史命令、运行身份、退出码和日志以各自result/process/Invocation文件为准。每次使用新GUID；编译输入变化后先成功执行新的完整Editor构建，再将它的result.json传给测试入口。固定旧RunId不会自动成为未来新源码的有效凭据。

```powershell
$optimizationWorkspace = 'E:\poject\feebooz\DivineBeastsWorkspace\.worktrees\v'
$optimizationEngine = 'F:\UnrealEngine-5.8.0-release'
$optimizationPwsh = 'E:\poject\feebooz\DivineBeastsWorkspace\Saved\Tools\PowerShell-7.6.6\pwsh.exe'

# 正常Server目标；Client复验将-Server替换为-Client，并使用新RunId。
& $optimizationPwsh -NoProfile -NonInteractive -File "$optimizationWorkspace\Build\Game\BuildFoundation.ps1" `
  -EngineRoot $optimizationEngine -Server -NoUBA '-NoSharedPCH:$false' `
  -MaxParallelActions 2 -TimeoutSeconds 14400 -RunId ([guid]::NewGuid())

# 正常Server成功收据、完整Editor成功凭据和真实开发定义均具备后，执行选定的Server Cook。
& $optimizationPwsh -NoProfile -NonInteractive -File "$optimizationWorkspace\Build\Game\CookFoundation.ps1" `
  -EngineRoot $optimizationEngine -Cook -Target Server -CustomConfig FoundationStandalone `
  -Pak -SkipDebugSymbols -TimeoutSeconds 7200 -RunId ([guid]::NewGuid())
```

完整UE自动化入口是 `E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v/Build/Validation/TestPluginArchitectureUnreal.ps1`，必须显式传入`-Execute -BuildResultPath`，可用`-Suites`定向重验。前置核验失败时应修正源码/产物对应关系并重建，不能将其他工作区路径或历史DLL写入新成功记录。

清单静态验证入口是 `E:/poject/feebooz/DivineBeastsWorkspace/Tests/Architecture/ValidatePluginDeliveryManifest.py`；其通过只表示描述、登记和证据文件存在性满足合同，完整业务场景仍按清单状态和实际运行结果判断。
