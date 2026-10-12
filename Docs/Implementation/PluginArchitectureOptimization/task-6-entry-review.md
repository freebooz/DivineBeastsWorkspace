# Task 6：插件架构验证入口独立复核

> 本报告前半保留初始只读审查快照。后续授权修复及各项关闭边界见末尾“后续授权修复与复验”；不得把原始发现当作修复后版本的结论。

## 范围与结论

只读复核`Build/Validation/TestPluginArchitectureUnreal.ps1`与`Tests/Architecture/ValidatePluginDeliveryManifest.py`，并读取其依赖`Build/Game/FoundationTools.psm1`、`Build/Validation/FoundationAutomationReport.psm1`、交付清单场景尾部及旧Data执行入口。未修改上述入口或共享台账，未启动UE、UBT或Cook。

结论：新入口有明确的失败分类、独占报告目录和33个真实测试身份，但还不能单凭本入口的Passed证明当前源码已被测试；交付清单校验也存在可以复现的状态/身份漏检。下面按影响排序给出具体触发条件。当前清单场景仍为NotRun/Blocked，本文不声称当前存在伪造的Passed记录。

本报告针对读取时快照：新UE入口read_revision=`7555604770030013`；清单校验器=`7555604770029818`；共用报告门禁=`7555604770029976`；FoundationTools=`7555604770029726`；清单=`7555604770029817`。Root收到早期发现后若已修复，应以新版本复验为准；本文不是对后续未读取代码的结论。

## 发现

### 1. P1：必需测试名存在不能排除同名旧DLL，当前源码验证可能误绿

位置：`Build/Validation/TestPluginArchitectureUnreal.ps1:25`、`:27`、`:68`、`:75`，以及`Build/Game/FoundationTools.psm1:104`。

入口只验证引擎版本、UnrealEditor-Cmd.exe和uproject存在，再运行测试并检查RequiredPaths。调用链没有检查受影响Editor/Runtime模块DLL是否由当前源码构建，也没有验证本次构建收据、源码指纹或DLL指纹。`Assert-FoundationFile`只检查文件存在且非空。

触发：先构建一个已经包含本轮33个测试名字的版本；之后修改生产代码或测试断言而保持名字不变；不重编，直接用`-Execute`运行。旧DLL仍能导出本次新目录的33个Success，现有门禁即可返回Passed。缺少新测试的更老二进制会被拒绝，但拥有同名测试的旧二进制不受此约束。

这是沿生产控制流得出的确定缺口，本次未实际运行旧DLL。入口说明第6行要求先构建当前Editor，如果此项由外部编排负责，结果必须携带并核对该构建证据；否则入口内应在启动前校验本次受影响模块的构建新鲜度。无需自动构建，也无需触碰现有编辑器。

### 2. P2：Passed只检查字段非空，不校验证据存在或运行身份

位置：`Tests/Architecture/ValidatePluginDeliveryManifest.py:47`。

实际只读负向试验：在内存复制真实清单，把`architecture`改为`Status="Passed"`、`RunId="not-a-run-id"`、`Evidence=["Saved/Validation/DoesNotExist/never-created-result.json"]`。调用真实`validate(root, manifest)`返回空错误数组。

因此维护者即使填错运行ID、写入不存在的路径，状态门禁仍把该声明视为结构一致。Evidence为非空字符串等错误结构也没有被逐项约束。建议至少要求合法运行ID、证据数组内的项目相对文件真实存在，并对机器结果核对RunId与成功状态；对于人工证据，应明确其独立的状态/签审合同。这里针对代码已经声明要拦截的“本次运行/证据”要求，不要求静态检查替代UE验收。

### 3. P2：物理同名插件被字典推导覆盖，清单可漏登实际描述文件

位置：`Tests/Architecture/ValidatePluginDeliveryManifest.py:51`，后续`:56`、`:83`。

实际临时夹具在Saved内创建两个不同物理路径的`ReviewDuplicate.uplugin`，清单只登记字典最后保留的一个路径；真实校验器仍返回空错误数组。原因是`{p.stem: p ...}`在检查前已经把两个相同stem折叠为一个条目。后续seen集合无法恢复丢失的描述身份。

这会使插件目录意外出现同名副本时，本入口仍显示manifest与实际插件一致。建议在构造身份字典之前按stem分组并明确拒绝多个物理描述；当前主工程本身未在本次只读检查中发现该重复，此发现来自受控夹具。

### 4. P2：foundation-data场景仍路由旧入口，未绑定本轮新增身份门禁

位置：`PluginDeliveryManifest.json:1249`；`Build/Validation/TestFoundationUnreal.ps1:33`；新入口`:31`。

清单的foundation-data场景Entry仍为`Build/Validation/TestFoundationUnreal.ps1`。该旧入口的Data必需集合只有9项；新入口为16项，多出的7项包括`Runtime.ResourceLeases`、4个Index用例、2个SourceIndexRegistry用例。旧入口虽然会请求Data前缀，但旧二进制只导出其原9项Success即可通过旧必需集合，因此不能自动兑现场景描述中“核对新增索引用例名称”的目标。

建议将该场景的自动化执行入口及明确参数关联到新入口的Data组，并保留原有地图、资源及人工步骤。PCG Gold和UI视觉场景的范围比本轮源代码Automation更大，不应把它们整体替换成较窄的新套件来宣称全部验收；可分别记录自动化子步骤。

### 5. P2：完整测试身份比较在后缀处忽略大小写

位置：`Build/Validation/FoundationAutomationReport.psm1:8`、`:11`、`:16`，由新入口`:75`消费。

实际执行共用函数：RequiredPaths为`GamePlatform.VFX.Diagnostics.MultiWorldCounts`，报告只有`GamePlatform.VFX.Diagnostics.multiWorldCounts`且state为Success，函数返回1并接受。Prefix用Ordinal比较，但`$seen=@{}`是默认不区分大小写的Hashtable，ContainsKey对完整身份并非精确Ordinal。

这与入口“固定完整身份”的规则不一致，也让只改后缀大小写的测试名称漂移无法被发现。建议使用明确Ordinal的字典/集合统一完整身份比较。对照试验已经确认，真正缺失的其他名称和Fail状态能被正确拒绝。

### 6. P3：正斜杠路径约束漏掉单个反斜杠

位置：`Tests/Architecture/ValidatePluginDeliveryManifest.py:18`。

判断使用的字符串代表两个连续反斜杠，因此Windows上的单反斜杠路径仍能通过。实际把MainProject改为`Game\DivineBeastsArena.uproject`，校验返回空错误数组，尽管报错合同声明必须使用正斜杠项目相对路径。建议改为拒绝任意一个反斜杠；不需要修改真实文件路径或资产身份。

## 保留模块检查的实际边界

校验器第79行只检查Maturity属于Reserved/SourcePresent，第90行检查全局ReservedModulePolicy。内存把当前明确登记为Reserved的`GamePlatformLocalizationClient`改成SourcePresent，仍返回空错误数组。

这说明当前门禁只验证字段枚举和全局声明，没有独立核对保留模块成员集合。由于SourcePresent本身明确不代表生产能力，本文不把该字段变化直接称为生产能力误报；但Root不应把本检查结果描述为已经验证所有保留模块分类准确。若本次任务要求保留既定成员集合，需让该集合有可核对的独立依据，而非只相信被校验的同一个字段。

## 已确认有效的保护

- `New-FoundationContext`拒绝复用同RunId/Operation证据目录；新入口只读取自己创建的suite/Automation/index.json，没有回退旧日志或最新历史报告的路径。
- 无Execute、缺少引擎/前置文件和Foundation引擎锁被占用，按FileNotFoundException映射NotExecuted/2；执行后无报告、无效JSON、缺失/失败测试走Failed/1。
- 引擎退出码非零时设置failedExternal，即使退出码为2也记录Failed，未被误写成NotExecuted；真实退出码保留。
- `Invoke-FoundationProcess`使用ProcessStartInfo.ArgumentList逐参数传递。带空格的ExecCmds、TestExit以及工程/证据目录保持独立argv，没有shell拼接。
- 33个RequiredPaths均在当前真实C++ Automation注册宏中找到，未发现缺名或重复注册；这只是源码身份核对，未证明DLL中实际发现这些测试。
- 报告门禁拒绝缺失身份与Fail状态。源文件/模块注册和场景当前NotRun/Blocked声明没有被本次报告升级为UE Passed。

## 只读/临时验证证据

证据根：`Saved/Validation/PluginArchitectureOptimization/20261012/Task6EntryReview/`。

|证据|实际执行内容与结果|
|---|---|
|`manifest-negative-cases.json`|通过run_script的Python解释器加载生产validate函数，真实清单作为对照返回[]；不存在通过证据、保留状态翻转、单反斜杠路径、物理同名描述夹具四个变异均未被拒绝。没有写真实清单。|
|`Fixtures/DuplicateIdentity/Manifest.json`及相邻夹具|仅Saved下的两个JSON插件描述、JSON工程/登记和普通说明文本；不包含伪造uasset/umap，也不参与实际Game/Plugins扫描。|
|`ReviewAutomationIdentity.ps1`、`automation-identity-cases.json`|使用`Saved/Tools/PowerShell-7.6.6/pwsh.exe -NoProfile -NonInteractive -File Saved/Validation/PluginArchitectureOptimization/20261012/Task6EntryReview/ReviewAutomationIdentity.ps1`调用真实报告函数；进程0，正常对照接受、缺失和失败拒绝、大小写变异被接受。未调用UE入口。|
|`required-test-source-identities.json`|Python只读提取新入口Required集合并检索真实Automation注册宏：33项，缺失0，重复0。没有重复运行此前已通过的功能套件。|

本次没有修改入口、依赖模块、PluginDeliveryManifest或共享执行台账。新增正式文件仅本报告，由Root登记总体目录。建议Root按修订后的具体代码对上述失败输入作定向复验，不把本报告的源码/脚本证据当作UE运行证据。

## 后续授权修复与复验（2026-10-12）

本节是Root在初始审查后明确授权的限定修复记录，前文保留原始发现、当时行号和失败证据。本轮只修改清单校验器、共用Automation报告门禁、foundation-data场景绑定及对应脚本回归；新UE runner与完整构建证据由另一代理负责，真实源码编译和UE执行由Root负责。本代理没有启动UE、UBT或Cook，没有关闭现有Editor，没有提交或推送。

### 修复范围与关闭状态

|原始发现|本轮结果|关闭边界|
|---|---|---|
|1：同名旧DLL不能代表当前源码|由另一代理实施BuildResultPath及构建证据门禁，本轮只协调并登记调用合同。|不由本报告宣称P1已完成运行验证；以其独立实现报告及Root实际构建/UE证据为准。|
|2：Passed身份/证据只检查非空|RunId必须为字符串且通过UUID/GUID解析；Evidence必须是非空列表，每项必须是非空规范相对路径，并在解析后保持工作区边界、实际存在。|限定的身份格式/路径存在性缺口已由脚本回归关闭；证据内容、成功状态和RunId归属仍不由静态清单校验器证明。|
|3：物理同名插件被字典覆盖|先按basename的casefold分组，保留全部物理描述；任何组有多个描述即报告全部冲突路径并拒绝。|完全同名与仅大小写不同、且分别完整登记的两个描述均有RED→GREEN夹具。|
|4：Data场景仍路由旧入口|Entry改为TestPluginArchitectureUnreal.ps1，步骤明确完整Editor构建、-Suites Data及-BuildResultPath，Expected明确16个完整必需身份。|场景绑定回归通过；场景Status仍是NotRun，Map/Profile未改，尚未把真实UE执行记为通过。|
|5：完整测试身份忽略大小写|用StringComparer.Ordinal构造Dictionary[string,bool]，原有Ordinal前缀筛选及精确Success状态判断保持。|错误大小写身份不能替代必需项；大小写不同的额外成功身份可被独立计数；缺失、重复、错误状态和额外失败仍拒绝。|
|6：单反斜杠未拒绝|所有受管路径拒绝任意反斜杠；同时拒绝绝对/盘符路径、点分量、父目录分量及重复/末尾分隔符。|MainProject单反斜杠和Evidence路径变异均有回归覆盖。|

证据路径可以指向真实文件或真实目录，因为本门禁只核对存在性。普通文本或存在的证据目录通过本检查，不等于其中有合格的运行报告。源码文件头、关键分支注释及命令行成功输出均明确这一边界；没有新增通用的运行报告解释器或人工签审推断。非Passed状态仍允许RunId为null及空Evidence，Blocked原有说明要求保持。

### 与新入口协商确认的Data调用合同

先通过现有入口完成同一工作区的完整Editor构建：

    Build/Game/BuildFoundation.ps1 -Editor -EngineRoot <锁定UE根> -RunId <BuildRunId>

不带-OnlyModules；Data已由现有主工程启用，不需要额外增加插件装配。UsePrecompiled、NoUBA等既有资源策略参数可由Root按实际构建情况明确选用并记录，不改变完整Editor资格。

随后执行：

    Build/Validation/TestPluginArchitectureUnreal.ps1 -Execute -Suites Data -EngineRoot <同一UE根> -BuildResultPath Saved/Validation/FoundationM0/<BuildRunId>/Build/result.json

按入口实现代理确认，BuildResultPath接受以runner工作区根解析的相对路径，或位于同一工作区内的绝对路径；不依赖调用方cwd。缺Execute/参数/证据文件应为NotExecuted/2；已提供但损坏、OnlyModules、异工作区/引擎、源码或DLL指纹不符的证据应为Failed/1且不启动UE；执行后的引擎非零退出仍保持Failed分类与真实退出码。此处记录的是已协调合同，本代理没有以未执行的runner替代其行为测试。实际通过还必须满足Data组16个完整身份逐一唯一Success。

### 实际修改文件

|文件|本轮修改|
|---|---|
|Tests/Architecture/ValidatePluginDeliveryManifest.py|GUID/证据形状、规范路径和存在性检查；casefold物理插件重名检查；中文职责与成功输出边界。|
|Build/Validation/FoundationAutomationReport.psm1|完整身份索引改Ordinal；保持原前缀、状态与错误流程。|
|Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json|仅foundation-data的Entry、三条Steps和Expected，共5处精确替换。|
|Tests/Foundation/Scripts/TestFoundationAutomationReport.ps1|保留已有7个回归，增加5个大小写/前缀/状态/计数回归。|
|Tests/Foundation/Scripts/TestPluginDeliveryManifest.py|新增11个unittest方法，调用真实validate API及真实清单场景绑定；临时文件不写正式插件树。|
|Docs/Implementation/PluginArchitectureOptimization/task-6-entry-review.md|保留原始审查并追加本节修复、证据及限制。|

本修复阶段新增正式文件仅Tests/Foundation/Scripts/TestPluginDeliveryManifest.py，由Root统一登记总体目录。本报告是前一审查阶段已创建的正式文件。本代理没有修改BuildFoundation、新runner、FoundationTools、ExecutionProgress或总体目录文档。

### RED→GREEN与最终证据

修复前观察HEAD为121823fa701f9a34cb2a806070438d70e05989e9，三个生产文件没有未提交差异；该HEAD相对于前面任务观察已由外部提交推进，本代理没有回退或创建提交。修改前原始字节/hash及git状态保存在：

Saved/Validation/PluginArchitectureOptimization/20261012/Task6EntryReview/Remediation/Before/

回归与增量核验证据根为同级Remediation目录。

|实际命令/证据|结果及含义|
|---|---|
|python Tests/Foundation/Scripts/TestPluginDeliveryManifest.py|RED退出1：旧实现产生28个失败断言（包括subTest，不是28项独立功能缺陷），明确复现非法身份/证据、同名插件与旧入口；GREEN退出0：11个方法中10个通过，1个符号链接用例明确跳过。|
|Saved/Tools/PowerShell-7.6.6/pwsh.exe -NoProfile -NonInteractive -File Tests/Foundation/Scripts/TestFoundationAutomationReport.ps1|RED退出1，错误大小写完整身份被接受；GREEN退出0，AutomationReportCases=12 Failed=0。仅内存报告对象，不运行UE入口。|
|python Tests/Architecture/ValidatePluginDeliveryManifest.py|退出0：plugins=64、code=47、content=17、reserved_modules=8；仅声明文档/登记及证据存在性一致。|
|red-results.json、manifest-red.log、automation-report-red.log|保留生产实现修改前的实际命令、退出码和完整失败输出。|
|green-results.json、manifest-green.log、automation-report-green.log、real-manifest-green.log|保留修复后的对应完整结果；没有把skip计入通过数量。|
|guarded-edit-plan.json、preservation-results.json、scoped-tracked.diff|4个既有代码/清单文件逐字节等于Before加各自精确授权编辑；新增测试与预定内容逐字节一致。PowerShell保留CRLF，Python/JSON保留原LF。|
|git diff --check -- 本轮5个代码/清单/测试路径|退出0。Git提示其autocrlf策略未来可能转换Python/JSON的LF，当前实际字节仍与Before原格式一致；未修改仓库换行配置。|

第一次辅助字节比较器没有把插入文本换行转换到既有PowerShell的CRLF，因此报告两项不匹配。检查实际字节后确认canonical编辑器已保留原CRLF；只调整了Saved中的比较逻辑，再按原格式重建期望字节，全部一致。初始比较结果保存在production-edit-preservation-before-newline-adaptation.json，未删除失败证据，也没有为比较器去改生产文件换行。

### 剩余限制与中文说明复核

- Windows创建符号链接返回WinError 1314，因此test_resolved_evidence_cannot_leave_workspace在本机明确skip。规范路径/父目录/绝对路径等用例实际执行；解析后工作区边界仍由生产代码检查。不能将该skip报告为本机符号链接绕出路径的动态验证成功。
- 本轮没有执行UE/UBT/Cook，也没有运行整个跨系统工程套件。实际执行范围是受影响的两组完整脚本回归及真实交付清单校验，未为数量重复此前无关通过项。
- GUID合法及证据存在不能证明证据内容真实、成功或属于该RunId。只有实际运行入口、完整构建证据关联及人工/设备验收才能补足各自结论。
- 保留模块成员集合仍没有独立事实源校验。当前Reserved计数为8是清单枚举结果，不是所有成员分类准确性的证明；本次限定授权没有改变该边界。
- 本轮人工复核了所改Python函数/路径与状态分支、Ordinal字典说明、测试目标及夹具范围、Data步骤和报告边界的中文注释；没有把局部整改表述为全工程中文说明已合规。
