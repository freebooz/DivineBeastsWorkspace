# SSLM演示运行来源与机器证据绑定

日期：2026-10-13。适用范围为方案A的T08/T11/T12开发工具，不是人工签收、发布证明或全插件覆盖报告。工具代码已写入，尚无真实UE运行证据提升；用户“不构建”限制持续生效。

## 职责与依赖

`Build/Game/FoundationBuildEvidence.psm1`继续唯一负责完整Editor结果、当前源码输入、实际收据及产物资格；新增`VerifiedModuleFiles`只输出本次已核对的必需模块实际路径/SHA256，不放宽原门禁。`PluginDemoEvidence.psm1`负责固定证据根、不可覆盖追加、严格JSON类型、实际文件摘要、内容端点、启动与原生报告的关联。`PublishPluginDemoEvidence.ps1`只在显式`-Publish`后消费这些材料，失败不改原始记录，不启动UE、不构建、不调用人工决定。

项目`DivineBeastsDemoClient/Private/Evidence/DivineBeastsDemoRuntimeOrigin`只观察当前进程参数及四个已加载运行模块文件，属于第三层开发模块，不创建平台资产管理器。`EditorGame -game`不会加载Type=Editor的`GamePlatformDeveloperTools`，不得要求它出现在运行观察或强载到游戏进程；该模块仍须在完整Editor构建及独立Review原生测试中取得证明。平台导入中立JSON与文件引用，不引用项目C++或资源。普通Client/Server不依赖Editor审核模块。

## 资格链与不可变输出

1. 受控EditorGame启动入口先要求匹配当前源码的完整Editor结果、新目录/界面资产及有效作者源。它捕获内容端点、重新核对构建，再启动自己的进程；启动记录绑定实际PID和启动时间，并传入唯一LaunchId、内容SHA256及禁热重载/禁编译参数。
2. 原始Writer始终保留`binary_verified=false`、`build_version/content_fingerprint=unverified`。附加`runtime_origin`不是自我授予资格：记录LaunchId、内容摘要、隔离请求和实际模块端点。UE5.8运行时消费的是`-LiveCoding=false`，不能把构建侧`-NoLiveCoding`当会话禁用；来源同时只读已存在LiveCoding模块的HasStarted/IsEnabledForSession/IsCompiling，缺模块或活动/曾启动会话拒绝，未编入该能力的目标明确按编译能力判断。热重载另检查ModuleManager实际文件名替换状态，不只信命令行文字。仍是端点观察，不是持续内存证明。
3. Demo与Review原生结果分别来自`TestPluginDemo.ps1 -Execute`；必须有完整当前必需测试，核对真实process/index、目录、PID/启动时间、项目/引擎/过滤器及前后构建指纹。进程必须有唯一`-ReportExportPath=<本次Suite/Automation>`，缺失/重复/其他目录均拒绝；固定目录有旧报告不等于该进程导出了它。组外用例不纳入本组证明，零测试、失败、超时、类型不正确或漂移均拒绝。
4. 提升工具读取原始状态文件并与索引嵌入状态比较；要求同Run/Scenario/Feature、已完成操作、合法终态及成功清理。预期失败可作为真实待审事实，不能被强改成成功。源JSON原始字节、启动前/提升时内容及实际模块必须匹配同一构建版本。
5. 验证通过后只追加新修订：完整`Content.<revision>.json`、有界`ContentSummary.<revision>.json`、`MachineInputs.<revision>.json`、`BinaryVerification.<revision>.json`和`VerifiedRunEvidence.<revision>.json`。最后一个是机器待审索引；旧raw、测试、截图和人工历史不覆盖。中途失败可能留下未被最终索引引用的诊断文件，不视为成功发布。
6. 平台导入保存实际索引、raw、证明、机器输入和内容清单摘要；签收前重读它们及必需证据。当前项目Editor桥仍只创建NotRun并以`unverified`当前内容上下文绑定；提升索引的完整导入/历史/当前版本刷新和签收界面仍待接入，不宣称人工闭环已可用。

固定来源目录为`Game/Saved/GamePlatformValidation/Launches/<LaunchId>/`；运行目录为`Game/Saved/GamePlatformValidation/<RunId>/Review/`。原生报告仍在现有`Saved/Validation/FoundationM0/<TestRunId>/PluginDemoTests/`。所有路径拒绝固定根逃逸和祖先重解析点；文件追加使用CreateNew，不自动删除旧修订。

Test入口通过`Write-PluginDemoTestOutput`最终收口：调用者要求的追加摘要先完成，然后发布canonical result。摘要已存在、写入失败或占用canonical路径时，最终工具及canonical均Failed，旧摘要保留；不能让脚本最终退出1而资格消费者仍读取Passed。该工具收口不改写原生用例的成功/失败事实。

## 内容版本范围与限制

`PluginDemoContentRoots-v1`保守覆盖项目`Game/Content`、每个真实项目插件`Content`、引擎`Engine/Content`及每个真实引擎插件`Content`的实际文件，包括外置Actor/对象数据。排序采用Ordinal，身份串包含规范路径、字节长度、SHA256；时间戳用于读取稳定性检查，不作为内容身份。SourceArt、缓存与Content外的动态资源不纳入此范围。

无关内容变化也会撤销资格。这种完整扫描的时间/内存成本尚未实测，不能作为性能优化结论；前后端点不锁住整个文件树、不证明运行期间的内存完全未变。模块诊断64MiB、平台JSON对象4MiB等上限是读入保护，不是批准性能预算；超限明确拒绝，不截断后伪称完整。完整内容清单单独绑定摘要，人工可见asset_manifest使用有界摘要。

UE5.8的动态模块文件名接口仅存在于非单体目标。单体目标输出`Demo.UnsupportedMonolithicOrigin`，不使用Editor DLL冒充Client可执行文件；Client启动和独立Stage/产物证明仍保持拒绝/待实现。已有Client Cook配置与Stage参数检查源码不等于Client实际产物通过。

这些材料都是无签名的本地端点证据，不是针对恶意外部输入的信任服务。提升只证明本地合同关联；没有审核人、HumanReviewed或Passed，也不能替代Cook、网络、颜色、性能与人工视觉验收。

## 后续实际执行入口

下列命令仅描述后续当前完整Editor及真实资产就绪后的工作流，**本批未执行Execute/Publish**。使用实际绝对路径替换示例值，不把旧日志改名当当前证据。

```powershell
pwsh -NoProfile -File Build/Game/TestPluginDemo.ps1 -Execute -EngineRoot <锁定引擎路径> -BuildResultPath <当前完整Editor结果> -Suite Demo
pwsh -NoProfile -File Build/Game/TestPluginDemo.ps1 -Execute -EngineRoot <锁定引擎路径> -BuildResultPath <同一完整Editor结果> -Suite Review
pwsh -NoProfile -File Build/Game/PublishPluginDemoEvidence.ps1 -Publish -EngineRoot <锁定引擎路径> -BuildResultPath <同一完整Editor结果> -DemoResultPath <真实Demo结果> -ReviewResultPath <真实Review结果> -RunId <原始运行GUID> -RawIndexPath <该运行Review根内原始索引>
```

Publish退出0只表示已追加`MachineVerifiedPendingHuman`索引，1表示验证/写入失败，2表示没有执行或缺必要前置材料。默认不带Publish不读产物/写索引。不得因工具返回0跳过当前版本重检、完整实施或集中人工核验。

## 当前测试边界

离线测试为`TestPluginDemoEvidence.ps1`（真实文件读取、纯值/类型/路径/内容漂移）、`TestPluginDemoNativeReport.ps1`（明确临时合成JSON的进程/输出目录/用例关联）、`TestPluginDemoTestOutput.ps1`（生产收口函数的实际临时文件成功/冲突/写失败）及`TestFoundationBuildEvidence.ps1`（完整Editor合同夹具）。夹具不启动UE、不制造资产、不写真实人工结论；实际数量/退出码见`ExecutionProgress.md`。

原生Evidence/Menu/ReviewEvidence回归源码已补，但未编译或运行。完整Publish成功/中途故障回归、真实启动/模块观察及Client/Stage路径尚待补证；源码静态通过不能消除这些风险。中文注释审核范围限本次工具、来源观察与CoreIdentity新增路径，不宣称全工程注释已合规。
