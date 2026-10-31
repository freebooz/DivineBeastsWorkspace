#requires -Version 7.0
<#
.SYNOPSIS
显式-Cook执行Development开发地图Cook及Stage；不触发C++构建或启动服务器。
.DESCRIPTION
Target为Client或Server，默认Client。Maps默认两张Foundation开发地图。
仅使用UE5.8真实BuildCookRun参数：-target、-client或-server -noclient、-CookOutputDir、-stagingdirectory。
独立新目录代替清理旧Cook；默认不迭代。不包含-build/-run/-clean。
Server先验证单源排除规则及受管INI漂移，UAT成功后逐个读取实际Stage/Pak/IoStore并拒绝纯表现泄漏。
返回0成功，1脚本失败，2未执行；外部非零退出码原样保留。
#>
param([string]$EngineRoot, [switch]$Cook, [ValidateSet('Client','Server')][string]$Target = 'Client',
    [string]$EditorBuildResult,
    # 保留Foundation默认行为；新手村开发验证必须显式选配置，不在发行默认中启用候选技能。
    [ValidateSet('FoundationStandalone','VillageServer','VillageDevelopmentServer','PluginDemoClient')][string]$CustomConfig='FoundationStandalone',
    [switch]$Pak,
    # 可选仅省略Stage中的PDB等调试符号，缓解磁盘不足；不改变构建模式、Cook内容或测试门禁。
    [switch]$SkipDebugSymbols,
    # 本机Zen不可用时显式使用原生文件Cook；仍保留Stage及服务器内容剥离门禁。
    [switch]$SkipZenStore,
    [string[]]$Maps = @('/Game/Development/Foundation/Maps/L_FoundationBootstrap','/Game/Development/Foundation/Maps/L_FoundationSandbox'),
    [ValidateRange(1,86400)][int]$TimeoutSeconds = 1800, [guid]$RunId = [guid]::NewGuid())
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
$context = $null; $lock = $null; $code = 2; $message = ''; $details = @{ Target=$Target; Maps=$Maps; Configuration='Development'; Platform='Win64'; CustomConfig=$CustomConfig; Pak=[bool]$Pak; SkipDebugSymbols=[bool]$SkipDebugSymbols; SkipZenStore=[bool]$SkipZenStore }
try {
    $context = New-FoundationContext "Cook-$Target" $RunId
    if (-not $Cook) { throw [IO.FileNotFoundException]::new('需显式指定-Cook。') }
    if ($CustomConfig -eq 'PluginDemoClient' -and $Target -ne 'Client') { throw 'PluginDemoClient只允许开发Client，不能用于任何服务器角色。' }
    if ($CustomConfig -eq 'VillageDevelopmentServer' -and $Target -ne 'Server') {
        throw 'VillageDevelopmentServer只允许专用服务器目标；客户端开发验证使用CookFrontEndClient。'
    }
    $engine = Get-FoundationEngine $EngineRoot
    $details.EngineVersion = $engine.Version; $details.EngineRoot = $engine.Root
    Import-Module (Join-Path $PSScriptRoot 'FoundationCookEvidence.psm1') -Force
    # 在UAT/引擎锁之前验证全部受管配置；配置漂移时不开始本次Cook，也不修改旧产物。
    $serverPresentationPolicy = $null
    if ($Target -eq 'Server') {
        $serverPolicyPython = Join-Path $engine.Root 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
        $serverPolicyTool = Join-Path $PSScriptRoot 'ServerPresentationPolicy.py'
        Assert-FoundationFile $serverPolicyPython; Assert-FoundationFile $serverPolicyTool
        $policyOutput = & $serverPolicyPython $serverPolicyTool check --custom-config $CustomConfig
        if ($LASTEXITCODE -ne 0) { throw '服务器排除配置漂移或真源无效；本次Cook未开始。' }
        $serverPresentationPolicy = ($policyOutput -join "\n") | ConvertFrom-Json
        $details.ServerPresentationPolicy = $serverPresentationPolicy
    }
    Assert-FoundationFile $context.Project
    Assert-FoundationFile (Join-Path $context.Workspace "Game/Source/DivineBeastsArena$Target.Target.cs")
    # Foundation地图白名单不能隐式扩大；只有Village专服配置可显式发布这一张已存在的正式村庄地图。
    # 不接受任意插件路径或组合地图，防止测试入口变成无边界CookAll；其余配置沿用原Foundation门禁。
    if ($CustomConfig -eq 'PluginDemoClient') {
        # 仅消费经过真实身份/关系校验的作者源地图集合，未知路径拒绝；不CookAll，不创建地图。
        $demoPython=Join-Path $engine.Root 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
        Assert-FoundationFile $demoPython
        $output=& $demoPython (Join-Path $context.Workspace 'Tests/Architecture/ValidatePluginDemoManifest.py') --workspace $context.Workspace --manifest (Join-Path $context.Workspace 'Docs/Implementation/PluginDemos/PluginDemoManifest.json') --print-cook-maps
        if ($LASTEXITCODE -ne 0) { throw '演示编目未通过地图白名单检查，拒绝启动Cook。' }
        $allowed=@(($output -join "`n") | ConvertFrom-Json)
        if (-not $PSBoundParameters.ContainsKey('Maps')) { $Maps=$allowed; $details.Maps=$Maps }
        if (-not $Maps.Count -or @($Maps | Where-Object { $_ -cnotin $allowed }).Count) { throw '演示Cook只允许作者源的精确地图集合。' }
    } elseif ($Target -eq 'Server' -and $CustomConfig -eq 'VillageDevelopmentServer' -and $Maps.Count -ge 1 -and
        @($Maps | Where-Object {$_ -cnotin @('/DBAWorldPack_Village/Maps/L_Village_Start','/DBAWorldPack_MainArena/Maps/L_DBA_MainArena')}).Count -eq 0) {
        # 显式开发联调仅增加已保存的MainArena白盒；其它Profile/任意插件路径仍被既有地图门禁拒绝。
        Assert-FoundationFile (Join-Path $context.Workspace 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_MainArena/Content/Maps/L_DBA_MainArena.umap')
        Assert-FoundationFile (Join-Path $context.Workspace 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap')
    } elseif ($Target -eq 'Server' -and $CustomConfig -eq 'VillageServer' -and
        $Maps.Count -eq 1 -and $Maps[0] -ceq '/DBAWorldPack_Village/Maps/L_Village_Start') {
        Assert-FoundationFile (Join-Path $context.Workspace 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap')
    } else {
        Assert-FoundationMaps $context $Maps
    }
    Assert-FoundationFile (Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe')
    $tool = Get-FoundationManagedTool $engine AutomationTool
    $lock = Enter-FoundationEngineLock $engine.Root
    $details.CookEditorGate=Assert-FoundationCookEditorBuildResult -Workspace $context.Workspace -EngineRoot $engine.Root -BuildResultPath $EditorBuildResult
    # UE单平台-outputdir不自动附加平台；Stage仅在目录末段匹配CookPlatform时使用原路径。
    $cookDirectory = Join-Path $context.Directory "Cooked/Windows$Target"
    if ($SkipZenStore) {
        # 文件Cook拼接插件长路径，沿用前端Cook已验证的独占短目录策略。
        $cookDirectory = Join-Path $context.Workspace ('Saved/C/'+$RunId.ToString('N').Substring(0,8)+"/Windows$Target")
        if (Test-Path -LiteralPath $cookDirectory) { throw '文件Cook独占短路径已存在，拒绝覆盖。' }
    }
    $stageDirectory = Join-Path $context.Directory 'Stage'
    $details.CookDirectory = $cookDirectory; $details.StageDirectory = $stageDirectory
    # Cook只由AdditionalCookerOptions选择角色/开发配置；不向UAT顶层传CustomConfig。
    # UBT ConfigCache仍会读取全局CustomConfig并改变Stage配置层；专服必须保留收据DedicatedServer剥离规则。
    $additionalCookerOptions = "-CustomConfig=$CustomConfig -ddc=NoZenLocalFallback"
    if ($SkipZenStore) { $additionalCookerOptions += ' -SkipZenStore' }
    $cookLocalCacheDirectory = Join-Path $context.Workspace 'Game/DerivedDataCache'
    $additionalCookerOptions += (' -LocalDataCachePath="'+$cookLocalCacheDirectory+'" -ini:EditorSettings:/Script/UnrealEd.EditorSettings:LocalDerivedDataCache=(Path="'+$cookLocalCacheDirectory+'")')
    $details.LocalDataCacheDirectory = $cookLocalCacheDirectory
    if ($Target -eq 'Server' -and @($serverPresentationPolicy.CookerArguments).Count -gt 0) {
        # FoundationStandalone仍保留原开发许可；只给Server Cooker追加逐目录+Key。
        # UE5.8 ConfigCacheIni按ArrayAdd处理独立token；不用同名Set、清数组或逗号拼接。
        $additionalCookerOptions += (' ' + ($serverPresentationPolicy.CookerArguments -join ' '))
    }
    $arguments = @($tool.Dll,'BuildCookRun',"-project=$($context.Project)","-target=DivineBeastsArena$Target",'-nop4','-unattended','-utf8output','-cook','-stage','-skipbuild','-skipbuildeditor','-platform=Win64','-clientconfig=Development','-serverconfig=Development',"-AdditionalCookerOptions=$additionalCookerOptions",'-ddc=NoZenLocalFallback',('-map=' + ($Maps -join '+')),"-CookOutputDir=$cookDirectory", "-stagingdirectory=$stageDirectory")
    if ($CustomConfig -eq 'VillageDevelopmentServer') {
        # Stage的AllowedConfigFiles先于收据CustomConfig目录过滤；只为显式开发发布加入两份运行INI。
        # 不改变正式Target、DedicatedServer的Pak规则或默认发行关闭状态；其他配置没有这些白名单。
        $developmentConfigFiles = @('DefaultGame.ini','DefaultGameplayTags.ini')
        $details.StagedDevelopmentConfigFiles = @()
        foreach ($configFile in $developmentConfigFiles) {
            Assert-FoundationFile (Join-Path $context.Workspace "Game/Config/Custom/$CustomConfig/$configFile")
            $stageConfigPath = "DivineBeastsArena/Config/Custom/$CustomConfig/$configFile"
            $arguments += "-ini:Game:[Staging]:+AllowedConfigFiles=$stageConfigPath"
            $details.StagedDevelopmentConfigFiles += $stageConfigPath
        }
        # 发布后启动还须指定这组参数；textconfig令实际进程重新读取已Stage的自定义INI层。
        $details.RequiredDevelopmentRuntimeArguments = "-CustomConfig=$CustomConfig -DBADevelopmentSkills -textconfig"

    }
    if ($CustomConfig -eq 'PluginDemoClient') {
        # Stage需保留显式开发INI层；只白名单本任务实际存在的文件，不启用发行默认或把全部Config搬入包。
        $configFile='DefaultGame.ini'
        Assert-FoundationFile (Join-Path $context.Workspace "Game/Config/Custom/PluginDemoClient/$configFile")
        $stageConfigPath="DivineBeastsArena/Config/Custom/PluginDemoClient/$configFile"
        $arguments += "-ini:Game:[Staging]:+AllowedConfigFiles=$stageConfigPath"
        $details.StagedDevelopmentConfigFiles=@($stageConfigPath)
        $details.RequiredDevelopmentRuntimeArguments='-CustomConfig=PluginDemoClient -DBAPluginDemo -DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub -textconfig'
        # 这里只描述所需运行参数，不启动Client；Client完整产物/源码证明与实际Cook仍须独立核验。
    }
    if ($Pak) { $arguments += '-pak' }
    # UE5.8 AutomationTool官方nodebuginfo仅控制Stage调试文件复制，运行二进制和资源仍完整校验。
    if ($SkipDebugSymbols) { $arguments += '-nodebuginfo' }
    if ($Target -eq 'Server') { $arguments += @('-server','-noclient','-serverplatform=Win64') } else { $arguments += '-client' }
    $tool.Environment.uebp_LogFolder = Join-Path $context.Directory 'UATLogs'
    $tool.Environment.uebp_EngineSavedFolder = Join-Path $context.Directory 'EngineSaved'
    # 与前端入口一致，仅覆盖本次UAT子进程环境，不修改用户/系统级TEMP或TMP。
    $cookTemporaryDirectory = Join-Path $context.Workspace ('Saved/T/'+$RunId.ToString('N').Substring(0,8))
    if (Test-Path -LiteralPath $cookTemporaryDirectory) { throw '本次Cook临时目录已存在，拒绝复用。' }
    $null = New-Item -ItemType Directory -Path $cookTemporaryDirectory -Force
    $tool.Environment.TEMP = $cookTemporaryDirectory
    $tool.Environment.TMP = $cookTemporaryDirectory
    $tool.Environment['UE-LocalDataCachePath'] = $cookLocalCacheDirectory
    $details.TemporaryDirectory = $cookTemporaryDirectory
    $result = Invoke-FoundationProcess -FilePath $tool.Executable -Arguments $arguments -WorkingDirectory (Split-Path $tool.Dll) -OutputDirectory $context.Directory -TimeoutSeconds $TimeoutSeconds -Environment $tool.Environment
    $code = $result.ExitCode; $details.Result = $result
    if ($code -eq 0) {
        # 工具退出0仍需核验实际Stage可执行文件，防止空输出误报成功。
        $executables = @(Get-ChildItem -LiteralPath $stageDirectory -Recurse -File -Filter "DivineBeastsArena$Target.exe" -ErrorAction SilentlyContinue | Where-Object { $_.Directory.Name -eq 'Win64' })
        if ($executables.Count -ne 1) { throw "Stage目标程序数量应为1，实际$($executables.Count)。" }
        $details.Executable = $executables[0].FullName
        if ($Target -eq 'Server') {
            # 真实Stage文件枚举与每个发现的Pak/IoStore清单同时审计；不复用历史CSV。
            # 策略工具检查必需容器存在、清单/工具错误及所请求地图；不证明全部载荷块完整性。
            $auditDirectory = Join-Path $context.Directory 'ServerPresentationAudit'
            $auditArguments = @($serverPolicyTool,'audit-stage','--profile',$serverPresentationPolicy.Profile,
                '--stage',$stageDirectory,'--unreal-pak',(Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealPak.exe'),
                '--evidence',$auditDirectory)
            foreach ($map in $Maps) { $auditArguments += @('--required-map',$map) }
            $auditOutput = & $serverPolicyPython @auditArguments
            $code = $LASTEXITCODE
            $details.ServerPresentationAuditReport = Join-Path $auditDirectory 'ServerPresentationAudit.json'
            if ($code -eq 0) { $details.ServerPresentationAudit = ($auditOutput -join "\n") | ConvertFrom-Json }
        }
    }
    if ($code -eq 0) {
        # Server及显式PluginDemoClient共用同一退休资源合同，演示只允许批准的terrain_review目标。
        $resourcePython=Join-Path $engine.Root 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
        $resourceGate=Join-Path $context.Workspace 'Tools/Unreal/Resources/ValidateResourceOwnershipContainer.py'
        Assert-FoundationFile $resourcePython; Assert-FoundationFile $resourceGate
        $resourceDirectory=Join-Path $context.Directory 'ResourceOwnershipGate'
        $null=New-Item -ItemType Directory -Path $resourceDirectory
        $resourceReport=Join-Path $resourceDirectory 'ResourceOwnership.json'
        $resourceResult=Invoke-FoundationProcess -FilePath $resourcePython -Arguments @($resourceGate,'--stage',$stageDirectory,'--unreal-pak',(Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealPak.exe'),'--profile',$CustomConfig,'--target',$Target,'--report',$resourceReport) -WorkingDirectory $context.Workspace -OutputDirectory $resourceDirectory -TimeoutSeconds 300
        $details.ResourceOwnershipGateResult=$resourceResult; $details.ResourceOwnershipReport=$resourceReport
        $code=$resourceResult.ExitCode
    }
    $message = if ($Target -eq 'Server') { '实际UAT Cook/Stage及服务器表现产物门禁结束；结果见独立容器证据，不代表运行或联机通过。' }
        else { '实际UAT客户端开发地图Cook/Stage结束；不代表运行通过。' }
} catch [IO.FileNotFoundException] { $code = 2; $message = $_.Exception.Message }
catch { $code = 1; $message = $_.Exception.Message }
finally { if ($null -ne $lock) { $lock.ReleaseMutex(); $lock.Dispose() } }
$status = if ($details.ContainsKey('Result') -and $details.Result.ExitCode -ne 0) { 'Failed' } else { '' }
if ($context) { Write-FoundationResult $context $code $message $details $status } else { Write-Host $message }
exit $code
