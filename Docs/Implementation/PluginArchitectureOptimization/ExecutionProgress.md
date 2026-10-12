# 插件架构优化执行记录

## 基线
- 用户授权：给出最佳实施计划后按计划执行。
- 起始HEAD：7d44194333cda24f8a14495695e1d478deef5a97；原分支main。本任务没有执行提交/推送。
- 2026-10-12约03:36 UTC共享工作区外部提交推进到f2cf1b6fcdcaa68ecb9c96e7cabef62fa4d0d740，后续推进到121823fa701f9a34cb2a806070438d70e05989e9。外部提交已包含部分本任务实施文件；保留提交及并发Surface/PCG差异，不回退、不将外部提交记为本任务操作。
- 已有工作区改动（其他PCG任务，保留）：
- `Docs/Implementation/GamePlatformPCG/ExecutionProgress.md`
- `Game/Plugins/GamePlatform/World/GamePlatformPCG/Docs/TestingAndEvidence.md`
- `Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCG/Private/Actors/GamePlatformPCGActors.cpp`
- `Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Authoring/GamePlatformPCGEditorLibrary.cpp`
- `Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGGoldMapAuthoring.cpp`
- `Tests/Architecture/VerifyPCGGoldAuthoring.py`
- `Tests/Architecture/VerifyPCGGoldSpatialProbe.py`
- `Tools/Unreal/PCG/README.md`
- `Tools/Unreal/PCG/RunGoldSpatialProbe.ps1`
- `Tools/Unreal/PCG/RunGoldDeterminism.ps1`

## 状态
- P0计划与边界：已保存并实施。
- Task 1服务器规则：源码及独立复核修复完成，25项Python回归、PS7.6下完整UBT配置回归通过；真实Cook/Stage待执行。
- Task 2索引：生产代码、6项Automation及文档完成；真实UE中6项新增索引测试全部通过。Data全组16项为14通过/2失败，根因是规范开发Probe资产在两工作区都不存在；保留原失败，修复既有创建入口并用UE准备夹具后重验。
- Task 3UI：资源租约/异步根布局、6项Automation和既有Description合同修复完成；完整Editor/Monolith只读回读已通过12个真实Profile、60条技能身份/文本与既有身份/图标审计，零错误。真实UI全前缀53项为42通过/11失败：3项新增RootLayout夹具/调用链待根因修复，其余涉及既有Introduction反射、拖拽方向、路由契约及真实RHI前置，分别归因，不以必需测试子集掩盖额外失败。
- Task 4接口与诊断：Arena接口收口完成；VFX重复创建子系统测试夹具、资源Pending漏报均已修复并复核，新增真实Data资源转换测试；实际UE验证待执行。
- Task 5交付证据：13包README、64插件/85模块台账、PCG BlueprintRoot参数化完成；静态回归及共享文件字节保护复核通过。root复核参数在Map/RepairMap副作用前校验、平台无项目默认值，其余阶段保持独立。
- Task 6统一验证：六项入口问题及独立Source范围缺口均已修复。清单/报告回归通过；完整Editor构建证据v2合同回归38/38、独立复跑38/38与13项针对性反例全部通过，四文件Delivery-v2冻结。见task-6-entry-review.md、task-6-build-evidence-report.md及独立review；最终builder的独立完整Editor构建5已实际通过并签发Eligible证据；旧混合失败批次不补证。实际UE发现的问题继续按原始报告修复/归因。
## 决策记录
- 原工作区增量修改，文件独占分工，保护已有PCG改动；不用隔离副本冒充当前项目。错误代价：共享文件协调遗漏可能冲突，使用版本栅栏与原始差异复核控制。
- 保留插件/资源身份，不删空壳或扩展第二工程。错误代价：暂时保留少量维护开销，后续以能力与使用证据决定去留。
- 正式业务内容按现有批准记录门禁，不把开发数据自动升级为正式。错误代价：资料缺失时正式资源交付仍待业务输入。
- Task3实际DefaultRootLayoutClass为cpp内const软类路径，不是规划中误记的UPROPERTY；保留既有路径/反射身份，不新增配置字段。错误代价：仍按既有代码位置调整布局路径。
- 复核AnimationHUDDevelopmentValidation的2026-10-11授权仅针对开发验证，尚无正式批准；保留正式内容门禁。
- 项目级准备官方PowerShell ZIP宿主，不改系统PATH/注册表。已验证7.6.6/.NET10.0.12可加载实际UE5.8 Build程序集；7.4/.NET8下PackagingConfig退出2是前置失败，不计通过。
- 已向现有PCG Session发送文件/编辑器协调问题，尚无回执；不关闭该编辑器，涉及共享文件须保留当前字节并做精确版本栅栏编辑。

- 验证隔离补充：正式实现仍只在原工作区维护。原Editor PID28168实际锁定Data/UI/PCG/VFX DLL，不能覆盖重链；managed worktree接口因managed_worktree_root_unavailable无副作用失败后，使用标准git worktree在已忽略的.worktrees/plugin-check-20261012创建固定f2cf检出并成功登记。该目录仅消费冻结的已审查源码快照，不是第二主工程/DevHost；所有验证必须标明其HEAD、补丁及源指纹，不能冒充变化中的原检出。保留旧验证产物与原Editor。
- 新增Build/Validation/TestPluginArchitectureUnreal.ps1，统一Data/UI/VFX/PCG新增及受影响Automation，要求33项具体测试身份。入口现要求-BuildResultPath；完整成功Editor、构建前后源码、实际receipt/DLL/.modules、引擎工具身份相符方可启动UE，执行结束再复核。范围不包括资产、SDK和全部引擎源码/运行库，不将本机未签名证据扩展成编译器远程证明。

## 验证证据
- 最新ValidatePluginDeliveryManifest.py实际exit0：64插件、47代码、17内容、8保留模块；清单回归10通过、1因Windows文件符号链接权限WinError1314明确跳过，未扩大权限。FoundationAutomationReport精确身份回归12通过；证据Saved/Validation/PluginArchitectureOptimization/20261012/Task6EntryReview/Remediation。清单校验仅证明登记与证据存在性一致。
- 最新TestServerPresentationPolicy.py：25项实际通过，exit0；包括权威目录大小写/祖先路径保护与UE原生/UBT INI语义回归。
- 最新TestFoundationPackagingConfig.ps1完整入口：PS7.6.6，EngineRoot=F:/UnrealEngine-5.8.0-release，exit0；Saved/Validation/FoundationM0/7369bbdb-63b9-44c7-bf71-db0cb24eb32c/PackagingConfig/result.json。配置回归不等于Cook。
- PCG静态回归VerifyPCGGoldAuthoring.py、VerifyPCGGoldSpatialProbe.py及PowerShell参数AST校验实际exit0；交付快照Saved/Validation/PluginArchitectureOptimization/20261012/Task5PCG/Delivery/manifest.json，源文件与保护快照一致性见同目录上级preservation-results.json。
- Editor构建1：RunId=20261012-0400-4000-8000-000000000001，exit6，长路径超过260限制，未进入C++编译。验证worktree通过git移动到授权根内更短的.worktrees/v后重新登记；曾尝试登记授权根外的兄弟目录被拒绝，未在那里执行验证，立即移回授权范围。
- 当前验证根E:/poject/feebooz/DivineBeastsWorkspace/.worktrees/v，detached HEAD=f2cf1b6fcdcaa68ecb9c96e7cabef62fa4d0d740；精确消费后续VFX/PCG修补和并发Surface两文件。1,554个编译输入与原检出121823fa版本逐一核对一致（仅规范CRLF→LF）；聚合SHA256=8811da75c306e1ccdb6e2fca4c8a32733a3cf978fc8f810cc33db84d31060b1b；SourceSnapshot-Editor2-Normalized.json保存在本任务Saved目录。
- Editor构建2：RunId=20261012-0400-4000-8000-000000000002，exit6；本轮C++编译无编译器错误，8个链接动作因GameplayTasks/ImageCore/Landscape/PhysicsCore/UMG的引擎导入库缺失而失败。随后严格使用5项真实UBT .lib.rsp、既有对象文件及MSVC lib.exe恢复缺失导入库，5项exit0，全部对应引擎DLL的SHA保持不变；证据在验证检出Saved/Validation/PluginArchitectureOptimization/20261012/EngineImportLibraries。
- Editor/Client/Server构建3：RunId=20261012-0400-4000-8000-000000000003。Editor完成9个链接/元数据动作并exit0；Client因缺少Sockets.precompiled而exit6，Server未启动，整个Build为Failed。Client/Server将采用正常编译模式，不将UsePrecompiled失败计为源码错误或通过。
- 新构建证据合同v1阶段34/34脚本回归exit0，2026-10-12T04:34:59.9491737Z；Saved/Validation/FoundationM0/eb391e13-81f7-46f4-b503-ae2f447728c3/BuildEvidenceContractTests/result.json，明确FixturesOnly=true、ActualBuildExecuted=false、UEExecuted=false。只读原receipt兼容探测不生成合格构建证据。
- UI实际只读回读尝试1：UE-Readback-1.log共960行，最后2026-10-12T04:27:52.427为TurnkeySupport模块加载记录；约5分钟无日志进展，root通过本任务确切job结束其拥有的进程，实际exit-1，未执行回读脚本，未生成资产回读通过结果。日志保留在验证检出Saved/Validation/PluginArchitectureOptimization/20261012/UIProfileReadback。
- Editor构建4：RunId=20261012-0400-4000-8000-000000000004。独立复核确认旧helper会漏掉Source内部Content/Docs子目录的新增C++；在UBT尚无编译动作且等待共享引擎时，root结束本任务确切job，exit-1，不签发证据。P1复现为13项中3项意外接受；实现已收窄到真实插件根排除并升级输入范围v2，最终回归及独立关闭进行中；见task-6-build-evidence-review.md。
- UI实际回读尝试2：子进程单独设置UE_SKIP_UBT_SDK_SETUP=1后越过启动阻塞，真实运行ReadUIProfiles.py；12份Profile/60条技能身份、显示名和非空白Description全部符合预期，附带既有身份/图标审计因60项does_asset_exist失败，使整体Status=Failed、UE退出-1。抽查图标物理文件存在且12英雄包已挂载，未把它当资源缺失或通过；该既有审计要求完整Editor/Monolith环境，正在按其前置重验。原始结果：验证检出Saved/Validation/PluginArchitectureOptimization/20261012/UIProfileReadback/Run-20261012T044632Z-54c7eebe/result.json；Invocation-2.json保留环境差异，SDK与Cook均未验证。
- 构建证据v2最终脚本证据：本轮ee396d8a-44d5-437b-8c72-0229c9ab221d与独立60794450-f78c-4cbd-a46a-b9bea1dba41e均38/38、exit0；原13项在Task6BuildEvidenceReview/FinalDelivery/result.json全部符合预期，零意外接受。runner临时副本仅在进程调用边界加入明确失败哨兵，保留真实前置门禁与Suite路径；全部为FixturesOnly，未执行UE/UBT。旧34项及37/38失败证据保留。
- UI完整Editor/Monolith回读3：先确认Monolith0.23.0、9316端口、1355动作、UI目录，以及PID21340的实际工程为验证检出Game/DivineBeastsArena.uproject、AssetRegistryLoading=false，再通过已发现schema的editor.run_python执行准备脚本。Run-20261012T051047Z-12c2246d/result.json于05:10:54 UTC为Passed：12/12 Profile、60/60条目/显示名/非空白Description、既有身份及图标审计通过，零错误。NativeValidateProfileInvoked=false，未写入/保存资产；NullRHI与开发内容范围不包含视觉、正式批准或Cook。此前Commandlet失败不再作为资产缺失判断。
- 另一Session的正常Client构建wc_job_3XMW1hSTb7-CbqYu在75/766动作处因2400秒限制自行timeout、exit-1；本任务未停止该进程。05:09 UTC只读确认无其UBT/编译器残留，原/v均无Client/Server完整目标产物；原项目留下部分Client对象缓存和历史Server缓存，可供UBT后续增量判断，不能替代本轮成功构建。
- 最终源对齐：SourceSnapshot-Editor5.json于05:17:02 UTC保存原HEAD121823fa及v固定HEAD；发现1个新增苔藓源码与11个雪地/配置/文档实质差异。逐项按该快照SHA精确同步到v，保留原工作区并发改动。实际Build5前清单与原冻结快照2533/2533条匹配：2207原字节一致、326由独立检查确认仅CRLF/LF。SourceSnapshot-Editor5-BuildInputAlignment.json为Passed；该对齐不承诺原项目在快照之后停止变化。
- UI字节一致复核：IconSnapshotComparison.json为Passed，60个图标文件两端SHA全部一致、读取前后与整批结束状态稳定；引用已有12个Profile字节一致证据。完整Editor回读所用资产与原快照对齐；该文件不是UE或视觉验收替代品。本次拥有的Editor通过Monolith QUIT_EDITOR正常退出，job wc_job_yPbtVO9AYGRUn7dd为exit0。正式只读回读结果、Monolith身份/schema、Invocation-3和完整UE日志已精确拷回原工作区本任务UIProfileReadback目录。
- 最终完整Editor构建5：RunId=20261012-0400-4000-8000-000000000005，job wc_job_qHLV9QxzlN-k_eUF实际exit0；05:23:52 UTC Build/result.json为Passed、EditorBuildEvidenceStatus=Eligible。UBT真实107.97秒、8个增量编译/链接/元数据动作。输入范围v2共2533条，构建前后不变；InputSHA256=5857E03F1819EFCEE7DD1FA40C8847FEBC2D1BDECFF15E2653EB40E6FFE0BFBE，项目产物SHA256=64AB0746344B41FB9EC8345A35F9573F31B5809F3715ADFBAC00B954B238C2A3。UsePrecompiled仅复用引擎依赖，完整项目Editor目标实际成功，不声称本次重链全部DLL。
- 统一UE自动化6：RunId=20261012-0400-4000-8000-000000000006，job wc_job_vuzlLj8ukxFd2QWP，实际exit1；Data原生UE退出0，但导出16项为14 Success/2 Fail，runner正确拒绝并未继续其他组。6项新增索引全部Success；RealAssetLeases/ResourceLeases都在GetPrimaryAssetPath前置发现foundation.probe@1未注册。独立诊断确认original/v的规范Probe和Flow文件均不存在、不受git ignore隐藏，DefaultEngine/DefaultGame扫描配置及运行参数正确。诊断Saved/Validation/PluginArchitectureOptimization/20261012/DataAutomation6Diagnosis.{md,json}。未改断言或CustomConfig。
- UI自动化7：RunId=20261012-0400-4000-8000-000000000007，job wc_job_lct5un4KQNNT9ok6，实际exit1；本次导出53项为42 Success/11 Fail，完整失败报告保留。3项RootLayout请求计数为0正在排查；Introduction用例在反射字段前置就失败，尚不能归因于资产；3项拖拽、3项路由契约及Color.SourceToSlate真实RHI前置单独核对。全组采用NullRHI，不能当颜色验收。
- PCG自动化8：RunId=20261012-0400-4000-8000-000000000008，job wc_job_TQdQgI6zMS8bKbUe，绑定Build5串行执行PCG组，结果待完成。
- 并发配置更新后的真实UBT配置复验：RunId=d705423b-7021-468b-869b-06e908ee48f2，TestFoundationPackagingConfig.ps1实际exit0；3项既有数组合并、6组脚本参数/Stage白名单、3组服务器排除数组全部通过。没有启动UE、UBT构建或Cook，仍不计Cook通过。
- 必需夹具准备发现既有入口缺陷：CreateFoundationAssets.py的main传execution_mode而UnrealDefinitionEditor构造器未接收，Probe/Flow会在创建前TypeError。按最小参数转发修复并补真实入口离线回归；之后仅用实际完整UE创建既有白名单Probe/Flow，不生成地图、不覆盖已有资产。
- 上一轮只读基线检查、只编译部分模块、资产清单、开发内容批准均不计作本轮完整构建、真实Cook、视觉或正式业务内容验收。