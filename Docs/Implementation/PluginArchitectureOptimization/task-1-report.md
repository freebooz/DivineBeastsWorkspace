# Task 1 实施报告：统一服务器表现资源排除策略

## 状态与证据边界

**实施完成，Python 回归与默认完整 UBT 配置回归通过；真实 Cook/Stage、容器工具运行及 UE 运行验收待主代理统一执行。**

- 日期：2026-10-12（UTC）。
- 项目：agent:freebooz-4b15fc0c7cf14418:divinebeastsworkspace-888d2615。
- 工作区：E:\poject\feebooz\DivineBeastsWorkspace。
- 需求依据：Docs/Implementation/PluginArchitectureOptimization/task-1-brief.md。
- 本任务只增量修改 Task 1 所属文件，未提交、推送、启动 Editor/Cook/C++ 构建或关闭其他编辑器。保留其他任务的未提交变更。
- 完整 UBT 回归是加载本机 UE 5.8 托管配置解析程序集执行的脚本测试，没有执行 UnrealBuildTool C++ 构建命令。它证明配置合并行为，不能代替原生 Cooker 或容器验收。

## 文件清单

| 文件 | 操作与职责 |
| --- | --- |
| Build/Game/ServerPresentationExclusions.json | 新增。唯一规则真源，记录包挂载路径、实际源目录、有限 profile、存在状态与中文依据。 |
| Build/Game/ServerPresentationExclusions.schema.json | 新增。封闭 Schema v1，供编辑器及维护者检查字段和有限取值。 |
| Build/Game/ServerPresentationPolicy.py | 新增。标准库实现的语义验证、确定性生成、漂移检查、Cooker 参数生成及 Stage/Pak/IoStore 清单门禁。 |
| Build/Game/CookFoundation.ps1 | 修改。Server Cook 前验证规则和受管配置；保留 Foundation 开发许可并追加服务器专用排除；UAT 成功后运行实际产物审计。 |
| Game/Config/Custom/DedicatedServer/DefaultGame.ini | 修改。受管 NeverCook 段，含外置 Actor/Object 对应目录。 |
| Game/Config/Custom/VillageServer/DefaultGame.ini | 修改。补齐共同表现排除，保留原地图与 AlwaysCook。 |
| Game/Config/Custom/VillageDevelopmentServer/DefaultGame.ini | 修改。补齐 BirdFlock、Snow；保留开发技能权威定义、开发开关与地图；仅排除技能 UI 子目录。 |
| Game/Config/Custom/DedicatedServer/DefaultPakFileRules.ini | 修改。由同源生成精确目录规则，保留专服 receipt 的表现剥离边界。 |
| Tests/Foundation/Scripts/TestServerPresentationPolicy.py | 新增。21 项回归，覆盖实际配置缺漏、规则、漂移、路径及容器证据解析。 |
| Tests/Foundation/Scripts/TestFoundationPackagingConfig.ps1 | 修改。保留完整默认入口与既有 UBT 加载门禁；增加真实脚本参数构造及服务器数组合并回归，另提供明确标识的 ArgumentsOnly 模式。 |
| Docs/Implementation/PluginArchitectureOptimization/task-1-report.md | 新增。本报告。 |

未修改共享目录规划或执行台账；新增路径请主代理统一登记。未新增插件、模块、反射身份、资源包或二进制资产。

## 规则设计

### 有限映射与单一来源

JSON 包含 42 条 CookDirectories：41 条既有共同表现目录，加 1 条 VillageDevelopmentServer 技能 UI 子目录。StageOnlyDirectories 保留 20 个既有专服剥离目录。原 Heroes 和 DBAHeroPack_* 泛化匹配改为当前十二个英雄包的精确目录。

| Cooker CustomConfig | 服务器策略 profile | 受管 NeverCook 目录数 | 适用方式 |
| --- | --- | ---: | --- |
| DedicatedServer | DedicatedServer | 119 | profile INI。策略工具支持此映射；本轮不扩展 CookFoundation 的原有参数集合。 |
| VillageServer | VillageServer | 119 | profile INI。 |
| VillageDevelopmentServer | VillageDevelopmentServer | 122 | profile INI，含开发技能 UI 与外置对象目录。 |
| FoundationStandalone | DedicatedServer | 119 | 仅 Server Cooker 追加独立的 +DirectoriesToNeverCook 参数，继续使用原 FoundationStandalone 开发配置。 |

FoundationStandalone 也用于客户端，因此没有向其 DefaultGame.ini 写入服务器排除，也没有把其 CustomConfig 替换成 DedicatedServer。CookFoundation 的客户端路径不追加这些参数。服务器仍不向 UAT 顶层传递 CustomConfig，Stage 继续采用 DedicatedServer receipt；开发服务器既有的两项 AllowedConfigFiles 和显式运行参数保持原有行为。

三个 profile 共用 DedicatedServer Stage 规则，故最终产物审计使用已确认表现目录的并集，包括开发技能 UI。生成后的 Pak 规则共有 142 条精确目录模式。

源码中的 Common/十二英雄当前均为表现包；英雄权威定义位于 DBAGameplay。保留既有整体 Stage 剥离没有扩大到世界、权威定义、碰撞或导航目录。JSON 将缺少实际目录的 DBASFXPack_Core 历史 Cook/Stage 项标为 LegacyAbsent，没有创建或登记这个包。

### 确定性生成与漂移门禁

执行器只允许三个固定服务器 Game 配置和一个固定 Pak 配置，使用明确的 BEGIN/END 标记管理对应段。生成按排序、去重后的规则工作，保留原文件 BOM、换行、地图、AlwaysCook、开发技能开关及无关配置。

- 拒绝未知 schema/profile/字段、重复 JSON 键、重复规则、目录通配、路径穿越、挂载与实际插件目录不对应、危险权威根目录，以及与实际存在状态不符的声明。
- 对子目录生成 __ExternalActors__ 和 __ExternalObjects__ 的对应挂载路径与源目录，保持地图外置对象的同一排除边界。
- 漂移检查不仅比较生成文本，还检查受管 section 外对同一数组的 Set、Clear、撤销或重复，以及对专服受管 Pak 段的覆盖。空白和大小写变化不能绕过；其他 section 的同名 key 不受影响。
- generate 先全量预检并复核原始字节，再逐文件原子替换；不宣称跨文件事务。中途终止留下的差异会在后续 Cook 前被 check 阻断。
- 本轮实际工程编辑通过 canonical edit_project_files 完成；没有通过运行生成脚本绕过源码编辑流程。

### 原生 UE 数组参数语义的核对

只读检查了锁定引擎：

F:\UnrealEngine-5.8.0-release\Engine\Source\Runtime\Core\Private\Misc\ConfigCacheIni.cpp

该文件在 ReadIniFileStrings 的命令行解析中逐 token 处理 -ini 参数；+Key 对应 ArrayAdd。代码同时明确指出一个 token 中逗号分隔多个带引号值存在解析限制。因此每目录使用独立、不含逗号与嵌套引号的参数，例如：

    -ini:Game:[/Script/UnrealEd.ProjectPackagingSettings]:+DirectoriesToNeverCook=(Path=/DBAUIPack_Core)

实际脚本构造回归核验了所有 119 个独立追加 token，不使用同名 Set 或清空数组；完整 UBT 回归又将全部 token 交给真实 ConfigCache.ReadHierarchy，并检查每一个要求目录仍在合并结果中。原生 ConfigCacheIni.cpp 只读核对与托管 UBT 执行结果相互补充；原生 Cooker 是否按预期处理仍以真实 Cook 后容器门禁为准。

## 实际产物门禁

CookFoundation 的 Server 分支先运行 check，漂移或规则错误会在 UAT/引擎锁之前结束。UAT 返回 0 后，还须存在唯一非空 Win64 服务器程序，随后才运行 audit-stage。

审计按本次真实 Stage 目录枚举所有文件，写 StageFiles.json；每个发现的 .pak 或 .utoc 都调用锁定 UnrealPak 重新获取清单，不接受历史 CSV 替代取证。IoStore 要求配对的非空 .ucas。证据输出目录必须新建并位于 Stage 外，审计前后文件大小/修改时间快照必须一致。

- Pak 使用 UnrealPak <实际 pak> -List，检查完整文件条目、数量和字节汇总、工具退出状态与错误输出。
- IoStore 使用 UnrealPak -ListContainer=<实际 utoc> -Csv=<本次新建 CSV>，检查实际字段、行结构、正大小、WindowsServer 资产记录和命名包数据。
- 全局 IoStore 元数据允许没有命名资源，但仍须有真实非空 Chunk；整体资产清单必须有非空包数据，且包含本次请求的全部地图。
- 路径按目录段边界比较，统一大小写和分隔符；Foo 不会匹配 Foobar。兼容虚拟挂载、实际插件/项目内容路径、外置 Actor/Object 路径及容器常见的 ../../../ 前缀；拒绝内部路径穿越。
- 输出 ServerPresentationAudit.json，包含原生命令、退出状态、已发现/已检查容器数与结果；RuntimeVerified 固定为 false。超时和取消只涉及本次启动的容器检查进程。

只读核对的引擎实现还发现：IoStoreUtilities.cpp 的 -ListContainer 分支可以在 UTOC 读取失败、仅写 CSV 表头后仍返回 0。执行器因此不能只检查退出码，必须拒绝空行集或无真实包数据的证据。CSV 字段和允许的元数据行为依据同一引擎的实际实现；Pak 行和汇总格式依据 PakFileUtilities.cpp。

本门禁证明资源路径排除、清单完整性约束与要求地图存在。它不执行全部 UCAS/Pak 载荷哈希或解压校验，也不证明权威资源在游戏中的功能、视觉、性能或联机结果。

## 实际验证记录

以下命令均在 E:\poject\feebooz\DivineBeastsWorkspace 执行。成功状态来自真实命令完成及输出，没有用静态检查替代执行。

### 1. 首次 RED：暴露既有缺漏

    F:/UnrealEngine-5.8.0-release/Engine/Binaries/ThirdParty/Python3/Win64/python.exe Tests/Foundation/Scripts/TestServerPresentationPolicy.py ConfigCoverageTests

退出码 1，2 项测试，其中共同覆盖测试的两个 profile 子例失败。实际输出表明 VillageServer 缺 38 项、VillageDevelopmentServer 缺 5 项；开发权威内容/地图保留测试通过。开发 profile 缺少的五项为：

- /DBAPresentationPack_Core/Materials/Snow
- /DBAPresentationPack_Core/VFX/Environment/BirdFlock
- /Game/Development/Snow
- /GamePlatformSurface/Textures/Snow
- /GamePlatformVFX/Environment/BirdFlock

### 2. 规则 GREEN 与补充 RED/GREEN

    F:/UnrealEngine-5.8.0-release/Engine/Binaries/ThirdParty/Python3/Win64/python.exe Tests/Foundation/Scripts/TestServerPresentationPolicy.py

初次实现 18 项通过。补充 section 外覆盖、无关 section 保留和共享 Stage 开发 UI 回归时，先得到 4 个失败及 1 个预期行为不符的异常，再修正 section 解析与共享 Stage 规则。最终完整执行结果：

    Ran 21 tests in 0.128s
    OK

退出码 0。覆盖三个实际 profile、Foundation 有限映射、未知/危险规则、外部对象、目录边界、受管段幂等与漂移、同名数组追加、允许权威定义/技能/碰撞/导航/必要动画路径、空 Stage、空/损坏/错误目标 CSV、缺失/错误 Pak 汇总和禁入资源。容器解析案例是明确的测试夹具；本轮没有创建或宣称它们为真实 UE Cook 产物。

### 3. 实际脚本参数构造 RED/GREEN

    E:/poject/feebooz/DivineBeastsWorkspace/Saved/Tools/PowerShell-7.4.20/pwsh.exe -NoProfile -File Tests/Foundation/Scripts/TestFoundationPackagingConfig.ps1 -EngineRoot F:/UnrealEngine-5.8.0-release -ArgumentsOnly

CookFoundation 集成前退出码 1，实际消息为“真实CookFoundation缺少服务器策略参数装配。”证据：

Saved/Validation/FoundationM0/adfc1eb7-2a2c-4ef5-b914-daa075add309/PackagingConfig/result.json

集成后同一命令退出码 0，最终输出：

    PackagingConfig: Passed (exit 0) 6组真实脚本参数装配通过；仅ArgumentsOnly，未执行UBT配置解析、Cook或Stage。

最后一次 ArgumentsOnly 证据：

Saved/Validation/FoundationM0/9dec9e43-43a6-4955-b168-c364ec90036f/PackagingConfig/result.json

此模式读取实际 Cook 脚本 AST 并执行参数构造片段，覆盖两个客户端入口及三种服务器配置，明确不等同于默认 UBT 完整入口。

### 4. 完整 UBT 宿主问题及最终通过

首次使用 PS 7.4.20、无 ArgumentsOnly 的完整入口返回 NotExecuted / 退出码 2。消息为 Unable to find type [UnrealBuildBase.Unreal+LocationOverride]。反射诊断显示本机 UE 托管程序集依赖 System.Runtime 10.0.0.0，而 PS 7.4 使用 .NET 8。没有修改 #requires、UBT 类型/加载方式或测试断言来绕过。

失败证据：

Saved/Validation/FoundationM0/f289869f-21d3-4244-a4d3-4e80cb925dbb/PackagingConfig/result.json

主代理随后提供独立 PS 7.6.6 / .NET 10.0.12 宿主。最终实际执行：

    E:/poject/feebooz/DivineBeastsWorkspace/Saved/Tools/PowerShell-7.6.6/pwsh.exe -NoProfile -File Tests/Foundation/Scripts/TestFoundationPackagingConfig.ps1 -EngineRoot F:/UnrealEngine-5.8.0-release

退出码 0；结果文件时间为 2026-10-12T03:35:25.9824844Z。实际输出：

    PackagingConfig: Passed (exit 0) 3项既有数组合并、6组真实脚本参数/Stage白名单及3组服务器排除数组UBT回归通过；未执行Cook、Stage或游戏启动。

完整证据：

Saved/Validation/FoundationM0/c72d8863-276d-4c71-ac1e-87023620bb58/PackagingConfig/result.json

| 实际 Cooker 配置 | 策略要求目录数 | UBT 合并后的总排除项数 | 结果 |
| --- | ---: | ---: | --- |
| FoundationStandalone | 119 | 122 | 全部要求目录保留，Foundation AlwaysCook 保留。 |
| VillageServer | 119 | 123 | 全部要求目录保留。 |
| VillageDevelopmentServer | 122 | 125 | 全部要求目录保留，开发 Stage 白名单精确保留。 |

总数包含项目原有无关排除项，因此与策略目录数不同。默认发行不启用开发技能、不相关数组项未被清空等既有断言也通过。

### 5. 漂移与差异检查

    F:/UnrealEngine-5.8.0-release/Engine/Binaries/ThirdParty/Python3/Win64/python.exe Build/Game/ServerPresentationPolicy.py check --custom-config FoundationStandalone

退出码 0；四个受管配置与真源一致，输出配置 SHA256、有限映射及 119 个 Cooker 参数。哈希只标识配置快照。

    git diff --check -- Build/Game/CookFoundation.ps1 Build/Game/ServerPresentationPolicy.py Build/Game/ServerPresentationExclusions.json Build/Game/ServerPresentationExclusions.schema.json Game/Config/Custom/DedicatedServer Game/Config/Custom/VillageServer/DefaultGame.ini Game/Config/Custom/VillageDevelopmentServer/DefaultGame.ini Tests/Foundation/Scripts/TestFoundationPackagingConfig.ps1 Tests/Foundation/Scripts/TestServerPresentationPolicy.py

退出码 0，无输出。git status 的 Task 1 范围与本报告文件清单一致。未执行提交。

## 主代理验收接口

### 规则检查与受管段生成

只读检查四个受管配置：

    F:/UnrealEngine-5.8.0-release/Engine/Binaries/ThirdParty/Python3/Win64/python.exe Build/Game/ServerPresentationPolicy.py check --custom-config VillageDevelopmentServer

修改 JSON 后显式生成受管段，再执行 check 和两个测试入口：

    F:/UnrealEngine-5.8.0-release/Engine/Binaries/ThirdParty/Python3/Win64/python.exe Build/Game/ServerPresentationPolicy.py generate

工具拒绝缺失或重复标记，不自动迁移未知配置。check 返回 0 只代表配置一致；失败返回 1，取消返回 130。

### 真实 Cook/Stage 与自动容器门禁（本轮未执行）

在主代理准备好构建产物、引擎锁和磁盘空间后，使用现有入口。例如 Foundation Server：

    E:/poject/feebooz/DivineBeastsWorkspace/Saved/Tools/PowerShell-7.6.6/pwsh.exe -NoProfile -File Build/Game/CookFoundation.ps1 -EngineRoot F:/UnrealEngine-5.8.0-release -Cook -Target Server -CustomConfig FoundationStandalone -Pak

Village Server：

    E:/poject/feebooz/DivineBeastsWorkspace/Saved/Tools/PowerShell-7.6.6/pwsh.exe -NoProfile -File Build/Game/CookFoundation.ps1 -EngineRoot F:/UnrealEngine-5.8.0-release -Cook -Target Server -CustomConfig VillageServer -Maps /DBAWorldPack_Village/Maps/L_Village_Start -Pak

Village Development Server 使用相同命令，将 CustomConfig 明确设为 VillageDevelopmentServer。可按已有入口能力显式加 -SkipDebugSymbols 控制调试符号复制。脚本不包含 C++ build/run；-Pak 及项目实际 IoStore 设置决定容器形态，审计会检查所有实际发现的容器。

查看本次 Saved/Validation/FoundationM0/<RunId>/Cook-Server/result.json 及同目录 ServerPresentationAudit/ServerPresentationAudit.json。只有 UAT、实际可执行文件、所有容器清单和路径门禁都成功，Server Cook 入口才返回 0。失败时保留原始容器 stdout.log、stderr.log、process.json 和已产生的审计结果；不要用历史结果代替本次运行。

已有真实 Stage 可独立复核，以下尖括号为调用者必须填写的实际路径占位符：

    F:/UnrealEngine-5.8.0-release/Engine/Binaries/ThirdParty/Python3/Win64/python.exe Build/Game/ServerPresentationPolicy.py audit-stage --profile VillageServer --stage "<本次真实Stage目录>" --unreal-pak F:/UnrealEngine-5.8.0-release/Engine/Binaries/Win64/UnrealPak.exe --evidence "<位于Stage外的全新证据目录>" --required-map /DBAWorldPack_Village/Maps/L_Village_Start

### 尚待完成与维护注意

- 本次未执行真实 Cook、Stage 或 UnrealPak；容器门禁的真实引擎输出兼容性、服务器地图启动与联机功能由主代理后续验收。UBT 通过不能替代这些结果。
- Common/十二英雄整体 Stage 剥离延续当前已确认的纯表现归属。未来若加入权威必需动画或其他服务器资源，应先重新评审目录所有权及规则，不能沿用“整个包纯表现”的假设。
- DBASFXPack_Core 仍为历史缺失声明；若未来真实引入，必须更新存在状态、所有权与规则依据。
- 服务器必要数据的允许路径回归证明策略没有误拦这些目录，不证明实际产物必然包含全部权威依赖；本次请求地图有显式存在检查，完整玩法依赖仍须真实 Cook 与运行验证。
- 原工作区其他编辑器及其 DLL 锁不由本任务处理。没有为通过验证停用其他任务或更改引擎托管 DLL。
