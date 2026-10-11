# Fauna Client Cook 与 DedicatedServer 排除验证计划（2026-10-11）

## 状态与证据边界

本文件最初来自当前工作区和锁定 UE 5.8.0 源码的只读核对，下面保留经核实的执行入口；命令本身不能引用为通过证据。后续实际构建、Cook、官方 Registry 包名审计与 Client Stage 结果分别见 FaunaDelivery_20261011.md 和 FaunaContainerAuditValidation_20261011.md。工作区 E:/work/2026/DivineBeastsWorkspace，引擎 D:/UnrealEngine-5.8.0-release。

执行时由主任务确认当前 Editor/构建占用已经结束，串行使用同一引擎，保留独立 RunId、参数、退出码和完整日志。不要覆盖旧输出或沿用旧成功报告。

- **静态配置/依赖**：最小入口：本次审查；通过后可陈述：Client AlwaysCook、Server NeverCook/Pak 规则、模块目标门闩已登记；仍不能陈述：Cook 通过、资源已交付、Server 二进制完全不含模块。
- **WindowsClient 定向资源 Cook**：最小入口：第 1 节真实 commandlet、两个 Fauna 根；通过后可陈述：当前 Editor 类实现可为 WindowsClient 烘焙这些资源及依赖，结果以本次 cooked 数据和 shader 日志为准；仍不能陈述：完整客户端发布、三张正式地图 Cook/Stage、Client 可执行文件已重建、视觉通过。
- **正式 FrontEndClient Cook/Stage**：最小入口：现有 CookFrontEndClient.ps1；通过后可陈述：三张入口地图和配置指定软资源完成真实 Cook/Stage，且通过脚本现有 UI/鸟群门禁；仍不能陈述：Fauna 门禁已自动存在、Fauna 运行/视觉/联网通过。
- **Village DedicatedServer Cook/Stage + 排除**：最小入口：现有 CookFoundation.ps1 的 Server/VillageServer 单地图入口；通过后可陈述：该 Server 目标/该地图组合的本次 cooked 和 staged 内容检查结果；仍不能陈述：所有服务器角色/地图的全局保证，或仅凭 Pak 剔除声称未 Cook。

“完整客户端 Cook”仅指既有 FrontEndClient 三图和配置闭包，不是 CookAll。定向 Cook 保留硬、软依赖，只缩减主动请求根，输出可能超过 Fauna 自身资产数。

## 已核对的入口和配置

- Build/Game/CookFrontEndClient.ps1:8、27–48：要求显式 -Cook 和有效真实 UIColor RHI 报告；固定 DivineBeastsArenaClient、WindowsClient，-cook -stage -pak -skipbuild -skipbuildeditor。地图为 L_DBA_FrontEnd、L_DBA_CharacterStudio、L_Village_Start。
- 同脚本:68–102：检查 Client.exe 和主 utoc；导出 ClientContainer.csv，再运行 UI/鸟群门禁。当前没有 Fauna 专项容器门禁，鸟群 JSON 不能替代。
- Game/Config/Custom/FrontEndClient/DefaultGame.ini:23–25：AlwaysCook /GamePlatformVFX/Environment/Fauna 和 /DBAPresentationPack_Core/VFX/Environment/Fauna。
- Game/Config/DefaultGame.ini:42–45：Definition 扫描包含 Fauna Definitions，CookRule=Unknown。扫描本身不保证软资源打包。
- Build/Game/CookFoundation.ps1:2–9、26–56：支持 Client/Server，CustomConfig 仅 FoundationStandalone、VillageServer、VillageDevelopmentServer；Server + VillageServer 支持唯一正式地图 /DBAWorldPack_Village/Maps/L_Village_Start。不能直接传 FrontEndClient、DedicatedServer 或任意 Fauna 地图。
- Game/Config/Custom/FoundationStandalone/DefaultGame.ini:1–5：只解除 Foundation 排除并 AlwaysCook 该开发目录。普通 Foundation Client Cook 没有 Fauna 主动收录保证。
- Game/Config/Custom/DedicatedServer/DefaultGame.ini:24–30：NeverCook 平台/项目 Fauna 及 /Game/Development/Fauna。
- 同目录 DefaultPakFileRules.ini:7–21：Stage/Pak 规则排除整个 GamePlatformVFX 和 DBAPresentationPack_Core 纯表现插件。
- Game/Source/DivineBeastsArenaServer.Target.cs:7、14：Type=Server，CustomConfig=DedicatedServer。实际 Server.target 的 AdditionalProperties 含 Name=CustomConfig、Value=DedicatedServer；不在收据顶层。
- Game/Config/Custom/VillageServer/DefaultGame.ini:6–9 已在真实 Cooker 入口配置直接加入两个 Fauna 根及 /Game/Development/Fauna 的 NeverCook，不依赖 DedicatedServer 收据的后续 Stage 剥离。
- Game/Config/Custom/VillageDevelopmentServer/DefaultGame.ini:66–69 同样明确排除这三个目录。CookFoundation 根据所选 CustomConfig 传给 Cooker；Stage 仍使用 Server 收据的 DedicatedServer 规则。当前静态配置缺口已补齐，但真实 cooked 数据、registry 和最终容器仍须按第 3 节分别验收，不能从配置文字推定通过。

## 执行前提

完整 Client/Server Cook/Stage 使用 -skipbuild/-skipbuildeditor，必须有与当前生产源码匹配的 Editor 和对应 Client/Server 二进制、target 收据。Editor 构建通过不能代替 Client 构建通过；exe 的存在和时间戳也不证明包含本次修改。只读盘点看到了当前 Client.exe/.target 和 Server.exe/.target，本文没有验证源码一致性或运行它们。

执行者需记录对应构建日志、HEAD、dirty diff 哈希。FrontEndClient 的 RHI 报告交由现有验证脚本处理；不能伪造报告或绕过门禁后称为完整发布。以下为 PowerShell 7 示例：

~~~powershell
$faunaWorkspace = 'E:/work/2026/DivineBeastsWorkspace'
$faunaEngine = 'D:/UnrealEngine-5.8.0-release'
Set-Location -LiteralPath $faunaWorkspace
~~~

## 1. 最小 WindowsClient 定向资源 Cook

直接使用当前 Editor 的真实资产类，目标 WindowsClient，不构建、不 Stage，也不依赖 Client.exe。两条 CookDir 为物理目录；NoGameAlwaysCook/NoDefaultMaps 仅减少无关主动根。不要传 CookSinglePackage、SkipHardReferences、SkipSoftReferences 或 DisableUnsolicitedPackages。

参数核对锁定引擎 Engine/Source/Editor/UnrealEd/Private/Commandlets/CookCommandlet.cpp:334–375、443–499；OutputDir 的 [Platform] 合同见同目录上层 Private/CookOnTheFlyServer.cpp:7285–7326。这里使用 commandlet 的 OutputDir，不使用 UAT 的 CookOutputDir。

~~~powershell
Import-Module ./Build/Game/FoundationTools.psm1 -Force
$faunaAssetRun = [guid]::NewGuid()
$faunaAssetContext = New-FoundationContext -Operation 'Cook-FaunaAssetsClient' -RunId $faunaAssetRun
$faunaAssetLock = Enter-FoundationEngineLock $faunaEngine
try {
    $faunaAssetArgs = @(
        (Join-Path $faunaWorkspace 'Game/DivineBeastsArena.uproject'),
        '-run=Cook', '-TargetPlatform=WindowsClient',
        '-NoGameAlwaysCook', '-NoDefaultMaps',
        ('-CookDir=' + (Join-Path $faunaWorkspace 'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/Fauna')),
        ('-CookDir=' + (Join-Path $faunaWorkspace 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/Fauna')),
        ('-OutputDir=' + (Join-Path $faunaAssetContext.Directory 'Cooked/[Platform]')),
        ('-abslog=' + (Join-Path $faunaAssetContext.Directory 'Cook.log')),
        '-unattended', '-nop4', '-utf8output', '-stdout', '-ddc=NoZenLocalFallback'
    )
    $faunaAssetResult = Invoke-FoundationProcess -FilePath (Join-Path $faunaEngine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -Arguments $faunaAssetArgs -WorkingDirectory $faunaWorkspace -OutputDirectory $faunaAssetContext.Directory -TimeoutSeconds 1800
    Write-FoundationResult $faunaAssetContext $faunaAssetResult.ExitCode 'WindowsClient定向Fauna资源Cook；不是完整Client发布或运行验收。' @{ Result=$faunaAssetResult; Scope='AssetCookOnly'; ManualAssetAcceptance='Pending' }
    if ($faunaAssetResult.ExitCode -ne 0) { throw 'Fauna定向资源Cook失败，保留本次日志。' }
}
finally {
    $faunaAssetLock.ReleaseMutex()
    $faunaAssetLock.Dispose()
}
~~~

证据根：Saved/Validation/FoundationM0/<faunaAssetRun>/Cook-FaunaAssetsClient/。

保留 result.json、stdout.log、stderr.log、进程参数记录、Cook.log、Cooked/WindowsClient 实际输出、AssetRegistry/CookedAssetRegistry 和 cook metadata。定位本次两个 Definition、两个项目 Niagara System，检查真实编译/保存成功，没有 missing class/import、未编译 Niagara 或 shader 错误。不能只凭源 .uasset、空 registry 或零退出码判定全部资源成功。

NoGameAlwaysCook 主动绕过产品 AlwaysCook，只用于快速资源诊断；仍需第 2 节验证正式配置收录。

## 2. 正式 FrontEndClient Cook/Stage

~~~powershell
$faunaClientRun = [guid]::NewGuid()
& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File ./Build/Game/CookFrontEndClient.ps1 -EngineRoot $faunaEngine -Cook -CustomConfig FrontEndClient -UIColorRHIReport (Join-Path $faunaWorkspace 'Game/Saved/Validation/UIColorRHI/Latest.json') -RunId $faunaClientRun
if ($LASTEXITCODE -ne 0) { throw '正式Client Cook/Stage或既有门禁失败。' }
$faunaClientEvidence = Join-Path $faunaWorkspace "Saved/Validation/FoundationM0/$faunaClientRun/Cook-FrontEndClient"
~~~

本次 RunId 的最低产物：

- result.json：ExitCode=0、Status=Passed，实际 UAT 参数和日志完整。
- Stage/WindowsClient/DivineBeastsArena/Binaries/Win64/DivineBeastsArenaClient.exe。
- Stage/WindowsClient/DivineBeastsArena/Content/Paks/DivineBeastsArena-WindowsClient.utoc，及配套 ucas/pak；多容器时全部检查。
- ContainerInspection/ClientContainer.csv、ClientContainer.json、BirdFlockContainer.json：仅证明现有脚本各自门禁。
- 另生成 FaunaContainer.json，记录 CSV/utoc SHA-256、Platform=WindowsClient、第 4 节 20 个运行包的 missing 列表、开发图/源美术 forbidden 列表，以及 source/build 身份。当前脚本未自动生成此报告。

新 RunId 不复用旧 cooked 目录，但合法 DDC 可以复用。不要添加 -iterate 后仍称为本次干净完整 Cook。

## 3. 最小 Village DedicatedServer Cook/Stage 与排除

当前 VillageServer 和 VillageDevelopmentServer 实际 Cook 配置已各自登记两个 Fauna 根与开发审核目录的 NeverCook。本节仍要求真实 cooked 数据和最终 staged 容器分阶段验证。

先核对收据的 AdditionalProperties；不要检查不存在的顶层 CustomConfig。

~~~powershell
$faunaServerReceipt = Get-Content -Raw ./Game/Binaries/Win64/DivineBeastsArenaServer.target | ConvertFrom-Json
$faunaServerConfig = @($faunaServerReceipt.AdditionalProperties | Where-Object Name -eq 'CustomConfig')
if ($faunaServerConfig.Count -ne 1 -or $faunaServerConfig[0].Value -ne 'DedicatedServer') {
    throw '当前Server收据不是DedicatedServer发布配置。'
}
$faunaServerRun = [guid]::NewGuid()
& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File ./Build/Game/CookFoundation.ps1 -EngineRoot $faunaEngine -Cook -Target Server -CustomConfig VillageServer -Maps '/DBAWorldPack_Village/Maps/L_Village_Start' -Pak -SkipDebugSymbols -RunId $faunaServerRun
if ($LASTEXITCODE -ne 0) { throw 'Village Server Cook/Stage失败。' }
$faunaServerEvidence = Join-Path $faunaWorkspace "Saved/Validation/FoundationM0/$faunaServerRun/Cook-Server"
~~~

分阶段接受条件：

1. result.json、stdout/stderr、UATLogs 和参数记录完整；目标 WindowsServer，实际 -server -noclient。Stage 有真正 Server.exe 和 L_Village_Start 运行包，确保不是空产物。
2. Cooked/WindowsServer 的 cooked package 清单与 cooked registry 不包含平台/项目 Fauna 和 /Game/Development/Fauna。另报告 Cook 是否加载/处理过 Fauna 源包。不要仅看可见 .uasset，遗漏其他 cooked 存储形式。
3. Stage/WindowsServer 所有实际 utoc/pak、loose files 均没有 Fauna 运行包和开发审核图外置 Actor/Object 包。按包路径/容器文件路径检查，不只查类名。
4. 导出真实 IoStore 清单。主和分块容器都必须覆盖；示例排除专用 global.utoc，不把其空清单当作游戏容器。缺失或解析错误记 Incomplete/Failed。

~~~powershell
$faunaServerInspection = Join-Path $faunaServerEvidence 'FaunaContainerInspection'
$null = New-Item -ItemType Directory -Path $faunaServerInspection
$faunaServerContainers = @(Get-ChildItem (Join-Path $faunaServerEvidence 'Stage/WindowsServer') -Recurse -File -Filter '*.utoc' | Where-Object Name -ne 'global.utoc')
if ($faunaServerContainers.Count -eq 0) { throw '未找到Server IoStore容器，不能执行该项验收。' }
$faunaContainerIndex = 0
foreach ($faunaContainer in $faunaServerContainers) {
    $faunaCsv = Join-Path $faunaServerInspection ("ServerContainer-{0}.csv" -f $faunaContainerIndex++)
    & (Join-Path $faunaEngine 'Engine/Binaries/Win64/UnrealPak.exe') ("-ListContainer=" + $faunaContainer.FullName) ("-Csv=" + $faunaCsv)
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $faunaCsv)) { throw 'Server IoStore清单导出失败。' }
}
$faunaPakIndex = 0
foreach ($faunaPak in @(Get-ChildItem (Join-Path $faunaServerEvidence 'Stage/WindowsServer') -Recurse -File -Filter '*.pak')) {
    $faunaPakLog = Join-Path $faunaServerInspection ("ServerPak-{0}.txt" -f $faunaPakIndex++)
    & (Join-Path $faunaEngine 'Engine/Binaries/Win64/UnrealPak.exe') $faunaPak.FullName -List *> $faunaPakLog
    if ($LASTEXITCODE -ne 0) { throw 'Server Pak清单导出失败。' }
}
~~~

FaunaContainer.json 应保存每个清单路径/SHA-256、Platform=WindowsServer、非空正式世界包计数和 forbidden。ValidateBirdFlockContainer.py 的严格 CSV 原则可参考，但它只检查鸟群，不能直接当 Fauna 检查器。

如果 cooked 数据有 Fauna、最终 staged 容器没有，只能写“最终服务器发布包排除通过；Cook 阶段排除未通过或未生效”。未执行本节就写“静态规则已登记；真实 Server Cook/Stage 未验证”。

## 4. 最小运行资源清单

当前两个 Fauna 内容根共 21 个源 .uasset，以下 20 个是本次 Client 的独立运行包验收清单。实际 Definition 文件有 DA_DBA_VFX_Fauna_ 前缀；逻辑 ID 和文件名分别记录。

平台包根 /GamePlatformVFX/Environment/Fauna；容器完整后缀根：
/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/Fauna。

- `EffectTypes/NET_GP_Fauna_Ambient.uasset`：1 个运行包。
- `Materials/M_GP_Fauna_Master.uasset`：1 个运行包。
- `Materials/T_GP_Fauna_NeutralRoughness.uasset`：1 个运行包。
- `Niagara/NS_GP_Fauna_Base.uasset`：1 个运行包。

项目包根 /DBAPresentationPack_Core/VFX/Environment/Fauna；容器完整后缀根：
/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/Fauna。

### ButterflyAzure：8 个运行包

- `Blueprints/BP_DBA_ButterflyAzureHabitat.uasset`：1 个运行包。
- `Definitions/DA_DBA_VFX_Fauna_ButterflyAzure.uasset`：1 个运行包。
- `Materials/MI_DBA_ButterflyAzure.uasset`：1 个运行包。
- `Meshes/SM_DBA_ButterflyAzure.uasset`：1 个运行包。
- `Niagara/NS_DBA_ButterflyAzure.uasset`：1 个运行包。
- `Textures/T_DBA_ButterflyAzure_BaseColor.uasset`：1 个运行包。
- `Textures/T_DBA_ButterflyAzure_Normal.uasset`：1 个运行包。
- `Textures/T_DBA_ButterflyAzure_Roughness.uasset`：1 个运行包。

### FishSilver：8 个运行包

- `Blueprints/BP_DBA_FishSilverHabitat.uasset`：1 个运行包。
- `Definitions/DA_DBA_VFX_Fauna_FishSilver.uasset`：1 个运行包。
- `Materials/MI_DBA_FishSilver.uasset`：1 个运行包。
- `Meshes/SM_DBA_FishSilver.uasset`：1 个运行包。
- `Niagara/NS_DBA_FishSilver.uasset`：1 个运行包。
- `Textures/T_DBA_FishSilver_BaseColor.uasset`：1 个运行包。
- `Textures/T_DBA_FishSilver_Normal.uasset`：1 个运行包。
- `Textures/T_DBA_FishSilver_Roughness.uasset`：1 个运行包。

两个物种每类路径各 1 个，即每类合计 2 个、项目合计 16 个；加平台 4 个，客户端必需运行包共 20 个。

第 21 个源资产是平台 Niagara/NM_GP_FaunaArrayMotion.uasset。与已有鸟群门禁原则一致，Niagara 模块编辑器图不要求作为独立运行包出现，其编译结果由 System/Emitter 持有；正常裁剪不能误报漏包，模块源文件存在也不能代替 System/GPU 编译成功。

CSV 应严格解析唯一 Filename/Platform 列，规范化斜杠/大小写，按完整后缀匹配。Client 要求 20 个运行包且平台为 WindowsClient；Server 平台 WindowsServer、Fauna 运行包为零。拒绝源美术、/Development/Fauna/ 审核图及 __ExternalActors__/Development/Fauna、__ExternalObjects__/Development/Fauna 外置包。当前正式客户端 DefaultGame.ini 未专列 Fauna 开发目录 NeverCook，实际是否进入产物仍以真实引用和清单为准，本次不改配置。

如引擎明确剥离了清单中的其他包，先调查日志和合法运行依赖，不可静默减少 requiredCount 来让门禁通过。

## 5. DedicatedServer 代码依赖结论

本次检索 Game/Plugins 下全部 .Build.cs/.uplugin，GamePlatformVFXClient 的外部直接模块依赖只发现于项目客户端模块和 VFXEditor：

- **DivineBeastsPresentationClient.Build.cs:30–39，PrivateDependencyModuleNames**：目标门闩：DBAClient.uplugin:157–167，ClientOnly，Game/Client/Editor 白名单；结论：新增本地惊扰/诊断依赖不构成 Server 入口。
- **GamePlatformVFXEditor.Build.cs:18**：目标门闩：GamePlatformVFX.uplugin:19–23，Editor only；结论：不进入 Server。
- **GamePlatformVFXClient 自身模块**：目标门闩：GamePlatformVFX.uplugin:12–16，ClientOnly，Client/Editor 白名单；结论：执行器没有 Server 模块。

Server.Target.cs 启用 DBAServer、GamePlatformWeather、DBAArena、Village 包。DBAArena.uplugin:40–47 的 DBAClient 依赖限 Client/Editor；DBAServer.uplugin 不依赖 DBAClient/VFX。主模块 DivineBeastsArena.Build.cs:9–20 无 VFXClient。DBAPresentationPack_Core 在 DivineBeastsArena.uproject:174–176 限 Game/Client/Editor，其 VFX 插件依赖也限 Client/Editor。

静态链路未发现共享 Runtime 新增依赖把 VFXClient 带入 DedicatedServer 的具体缺陷。当前 Server receipt 的 Plugins/BuildPlugins 只读检索也未见 DBAClient、GamePlatformVFX、DBAPresentationPack_Core；收据可能早于本次源码，不作为本次 Server 构建通过证据。最终用本次 Server 构建收据、链接输入/模块清单和实际 Cook/Stage 补齐。运行 Cook 的 Editor 加载 VFXClient 不等于 Server 可执行文件携带该模块。

## 目录规划登记与本轮变更

由主任务统一登记；本文作者不改混合换行总目录计划：

- **Docs/Implementation/FaunaCookValidationPlan_20261011.md**：职责：既有 Client/Server Cook 入口、Fauna 定向 Cook、容器验收清单与静态规则证据边界；状态：计划已写；Cook/Stage 未执行。

本文作者本轮没有修改生产源码、配置、构建脚本、引擎文件或源资产；主任务补齐的两份 Server Cook 配置已在上文只读核对。另外按授权修正 Tests/VFX/BirdFlock 下两份 Owner 脚本的 Python 返回值判定、真实反射调用及 NEVER 运行态赋值桥接。本子任务 AST 和 git diff --check 通过；主任务生成的最终真实 PIE 报告已在 BirdFlockAudit_20261011.md 中单独登记，但不构成本文 Client/Server Cook 已执行的证据。
