#requires -Version 7.0
<#
.SYNOPSIS
显式发布登录、角色预览和Village客户端，并检查实际IoStore中的必需软引用资产。
.DESCRIPTION
只使用锁定UE5.8及现有Client二进制；不构建、不启动后端、不修改资产或复用旧Cook。
输入EngineRoot、-Cook、超时秒数和独占RunId；输出Saved验证目录、真实退出码及包内容报告。
通过专用FrontEndClient配置纳入UI/流程/英雄软引用，避免仅指定地图时发布黑屏客户端。
专用服务器不使用此入口；失败保留独立证据，回退可运行上一包，不删除或覆盖已有产物。
退出2表示未执行，1表示检查失败；引擎非零码原样保留。通过不等于新手村网络准入通过。
#>
param([string]$EngineRoot, [switch]$Cook,
    # 可显式绑定完整Editor构建；缺省核验最新成功完整Editor证据，拒绝陈旧反射DLL。
    [string]$EditorBuildResult,
    # 默认不向每个独占Stage重复复制数GB的PDB；原始调试符号仍归Game/Binaries。
    # 需要随包调试时显式传-SkipDebugSymbols:$false，不降低配置、不跳过Cook与资产容器检查。
    [switch]$SkipDebugSymbols=$true,
    # Zen服务实际不可用时显式回退原生文件Cook输出；不关闭IoStore、DDC或现有发布门禁。
    [switch]$SkipZenStore,
    # 文件Cook遇到深层资产名超过260字符时，显式使用独占短输出目录；不改变资产身份或跳过检查。
    [ValidatePattern('^[A-Za-z]:[\\/]SSLM-Cook[\\/][a-fA-F0-9]{8,32}$')][string]$ShortCookRoot,
    # 必须匹配当前公共UI与原图的真实RHI颜色报告；资源变化后旧报告拒绝发布。
    [string]$UIColorRHIReport,
    # 默认发行前端保持开发资源隔离；候选技能必须显式选择用户批准的开发客户端配置。
    [ValidateSet('FrontEndClient','VillageDevelopmentClient')][string]$CustomConfig='FrontEndClient',
    [ValidateRange(1,86400)][int]$TimeoutSeconds=1800, [guid]$RunId=[guid]::NewGuid())
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
$context=$null; $lock=$null; $code=2; $message=''; $details=@{Target='Client';CustomConfig=$CustomConfig;SkipZenStore=[bool]$SkipZenStore}
try {
    $context=New-FoundationContext 'Cook-FrontEndClient' $RunId
    if (!$Cook) { throw [IO.FileNotFoundException]::new('需显式指定-Cook。') }
    $engine=Get-FoundationEngine $EngineRoot
    $details.EngineVersion=$engine.Version
    $details.EngineRoot=$engine.Root
    Import-Module (Join-Path $PSScriptRoot 'FoundationCookEvidence.psm1') -Force
    $tool=Get-FoundationManagedTool $engine AutomationTool
    Assert-FoundationFile $context.Project
    Assert-FoundationFile (Join-Path $context.Workspace "Game/Config/Custom/$CustomConfig/DefaultGame.ini")
    if(!$UIColorRHIReport){$UIColorRHIReport=Join-Path $context.Workspace 'Game/Saved/Validation/UIColorRHI/Latest.json'}
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $PSScriptRoot 'TestUIColorRHI.ps1') -VerifyOnly -ReportPath $UIColorRHIReport
    if($LASTEXITCODE -ne 0){throw '真实RHI颜色门禁未通过；拒绝发布缺失、失败或过期的UI颜色证据。'}
    $details.UIColorRHIReport=[IO.Path]::GetFullPath($UIColorRHIReport)
    $lock=Enter-FoundationEngineLock $engine.Root
    # 锁内核验，防止预检后被另一构建替换Editor DLL，再启动UAT。
    $details.CookEditorGate=Assert-FoundationCookEditorBuildResult -Workspace $context.Workspace -EngineRoot $engine.Root -BuildResultPath $EditorBuildResult
    if($ShortCookRoot -and -not $SkipZenStore){throw 'ShortCookRoot只用于显式文件Cook（SkipZenStore）。'}
    $cookDirectory=Join-Path $context.Directory 'Cooked/WindowsClient'
    if ($SkipZenStore) {
        # 文件Cook会拼接完整插件目录，深层验证目录会触发Windows的260字符限制。
        # 仅此恢复模式使用工作区内独占短路径；不改源资产身份、不复用或清理旧Cook。
        # UAT要求最终目录仍以平台名结尾，否则Stage会再追加一层WindowsClient。
        $cookDirectory=Join-Path $context.Workspace ('Saved/C/'+$RunId.ToString('N').Substring(0,8)+'/WindowsClient')
        if($ShortCookRoot){
            $shortRoot=[IO.Path]::GetFullPath($ShortCookRoot)
            $shortParent=Split-Path -Parent $shortRoot
            if(Test-Path -LiteralPath $shortParent){
                if((Get-Item -LiteralPath $shortParent).Attributes -band [IO.FileAttributes]::ReparsePoint){throw '短Cook父目录不得为重解析点。'}
            }
            if(Test-Path -LiteralPath $shortRoot){throw '短Cook目录已经存在，拒绝覆盖。'}
            $null=New-Item -ItemType Directory -Path $shortRoot -Force
            @{Workspace=$context.Workspace;RunId=$RunId.ToString();Purpose='Exclusive frontend file Cook'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $shortRoot 'Owner.json') -Encoding utf8
            $cookDirectory=Join-Path $shortRoot 'WindowsClient'
            $details.ShortCookRoot=$shortRoot
        }
        if (Test-Path -LiteralPath $cookDirectory) { throw '文件Cook独占短路径已存在，拒绝覆盖。' }
    }
    $stageDirectory=Join-Path $context.Directory 'Stage'
    $details.CookDirectory=$cookDirectory; $details.StageDirectory=$stageDirectory
    # 三张真实地图与小范围软引用目录共同组成前端回归包；不使用CookAll扩大到全部开发内容。
    $maps=@('/DBAFrontEndPack/Maps/L_DBA_FrontEnd','/DBAFrontEndPack/Maps/L_DBA_CharacterStudio','/DBAWorldPack_Village/Maps/L_Village_Start')
    if($CustomConfig -eq 'VillageDevelopmentClient'){$maps+='/DBAWorldPack_MainArena/Maps/L_DBA_MainArena'}
    $details.Maps=$maps
    $cookOptions="-CustomConfig=$CustomConfig -ddc=NoZenLocalFallback"
    if ($SkipZenStore) { $cookOptions += ' -SkipZenStore' }
    $arguments=@($tool.Dll,'BuildCookRun',"-project=$($context.Project)",'-target=DivineBeastsArenaClient',
        '-nop4','-unattended','-utf8output','-cook','-stage','-pak','-skipbuild','-skipbuildeditor','-platform=Win64',
        '-clientconfig=Development','-client','-ddc=NoZenLocalFallback',
        "-AdditionalCookerOptions=$cookOptions",
        ('-map='+($maps -join '+')), "-CookOutputDir=$cookDirectory", "-stagingdirectory=$stageDirectory")
    if ($SkipDebugSymbols) { $arguments += '-nodebuginfo' }
    $details.SkipDebugSymbols = [bool]$SkipDebugSymbols
    if ($CustomConfig -eq 'VillageDevelopmentClient') {
        # Cook的CustomConfig不决定Stage：Stage仍按目标收据过滤Config/Custom目录。
        # UAT顶层CustomConfig又会被UBT配置缓存读取，影响其他Stage层，因此只精确白名单这两份开发INI。
        # DefaultGame含开关、十二集合映射和客户端UIProfile扫描；GameplayTags含36个独立冷却标签。
        $developmentConfigFiles = @('DefaultGame.ini','DefaultGameplayTags.ini')
        $details.StagedDevelopmentConfigFiles = @()
        foreach ($configFile in $developmentConfigFiles) {
            Assert-FoundationFile (Join-Path $context.Workspace "Game/Config/Custom/$CustomConfig/$configFile")
            $stageConfigPath = "DivineBeastsArena/Config/Custom/$CustomConfig/$configFile"
            $arguments += "-ini:Game:[Staging]:+AllowedConfigFiles=$stageConfigPath"
            $details.StagedDevelopmentConfigFiles += $stageConfigPath
        }
        # 此记录是发布后启动要求，不代表脚本已经运行游戏；textconfig绕过错误层级的BinaryConfig缓存。
        $details.RequiredDevelopmentRuntimeArguments = "-CustomConfig=$CustomConfig -DBADevelopmentSkills -textconfig"

    }
    $tool.Environment.uebp_LogFolder=Join-Path $context.Directory 'UATLogs'
    $tool.Environment.uebp_EngineSavedFolder=Join-Path $context.Directory 'EngineSaved'
    $result=Invoke-FoundationProcess -FilePath $tool.Executable -Arguments $arguments -WorkingDirectory (Split-Path $tool.Dll) -OutputDirectory $context.Directory -TimeoutSeconds $TimeoutSeconds -Environment $tool.Environment
    $details.Result=$result; $code=$result.ExitCode
    if ($code -eq 0) {
        $executable=Join-Path $stageDirectory 'WindowsClient/DivineBeastsArena/Binaries/Win64/DivineBeastsArenaClient.exe'
        $container=Join-Path $stageDirectory 'WindowsClient/DivineBeastsArena/Content/Paks/DivineBeastsArena-WindowsClient.utoc'
        Assert-FoundationFile $executable; Assert-FoundationFile $container
        $details.Executable=$executable
        # 独立读取最终IoStore，不把Cook退出0或磁盘源资产存在当作资源交付成功。
        $inspectionDirectory=Join-Path $context.Directory 'ContainerInspection'
        $null=New-Item -ItemType Directory -Path $inspectionDirectory
        $csv=Join-Path $inspectionDirectory 'ClientContainer.csv'
        $pakTool=Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealPak.exe'
        $inspection=Invoke-FoundationProcess -FilePath $pakTool -Arguments @("-ListContainer=$container","-Csv=$csv") -WorkingDirectory (Split-Path $pakTool) -OutputDirectory $inspectionDirectory -TimeoutSeconds 120
        $details.ContainerInspection=$inspection; $code=$inspection.ExitCode
        if ($code -eq 0) {
            $check=Join-Path $context.Workspace 'Tests/Assets/TestFrontEndClientContainer.ps1'
            $gateArguments = @('-NoProfile','-File',$check,'-ContainerCsv',$csv,'-ReportPath',(Join-Path $context.Directory 'ClientContainer.json'))
            if ($CustomConfig -eq 'VillageDevelopmentClient') { $gateArguments += @('-DevelopmentSkills','-DevelopmentArena') }
            & (Join-Path $PSHOME 'pwsh.exe') @gateArguments
            $code=$LASTEXITCODE
        }
    }
    if ($code -eq 0) {
        # 鸟群19项依赖与旧原型禁入是发布门禁，不能只检查UI后放行。
        # 使用锁定UE内置Python，避免PATH命中Windows商店占位解释器。
        $birdPython=Join-Path $engine.Root 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
        $birdGate=Join-Path $context.Workspace 'Tools/Unreal/VFX/BirdFlock/ValidateBirdFlockContainer.py'
        Assert-FoundationFile $birdPython; Assert-FoundationFile $birdGate
        $birdReport=Join-Path $context.Directory 'BirdFlockContainer.json'
        # Foundation进程助手要求调用方提前创建输出目录，否则连stdout日志都无法打开。
        $null=New-Item -ItemType Directory -Path (Join-Path $context.Directory 'BirdFlockGate')
        $birdResult=Invoke-FoundationProcess -FilePath $birdPython -Arguments @($birdGate,'--csv',$csv,'--target','Client','--report',$birdReport) -WorkingDirectory $context.Workspace -OutputDirectory (Join-Path $context.Directory 'BirdFlockGate') -TimeoutSeconds 60
        $details.BirdFlockContainerReport=$birdReport
        $details.BirdFlockGateResult=$birdResult
        $code=$birdResult.ExitCode
    }
    if ($code -eq 0) {
        # P7–P9：源音频导入及普通Cook退出0不证明声音已入包；检查真实IoStore四项正式依赖。
        # 与原UI颜色和鸟群门禁串联，任一失败均不得放行；开发审核对象及声音SourceArt禁止入包。
        $contentPython=Join-Path $engine.Root 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
        $contentGate=Join-Path $context.Workspace 'Build/Validation/PluginDeliveryEvidence.py'
        Assert-FoundationFile $contentPython; Assert-FoundationFile $contentGate
        $contentOutput=Join-Path $context.Directory 'P7ContentGate'
        $null=New-Item -ItemType Directory -Path $contentOutput
        $contentReport=Join-Path $contentOutput 'ClientContent.json'
        $contentResult=Invoke-FoundationProcess -FilePath $contentPython -Arguments @($contentGate,'--container-csv',$csv,'--target','Client','--report',$contentReport) -WorkingDirectory $context.Workspace -OutputDirectory $contentOutput -TimeoutSeconds 60
        $details.P7ContentContainerReport=$contentReport
        $details.P7ContentGateResult=$contentResult
        $code=$contentResult.ExitCode
    }
    if ($code -eq 0 -and $CustomConfig -eq 'VillageDevelopmentClient') {
        # 本轮新增软引用不能靠旧六系统门禁漏检；实际读取同一最终容器的19项动作/起手/石灵/中文字体闭包。
        $castingPython=Join-Path $engine.Root 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
        $castingGate=Join-Path $context.Workspace 'Tests/Assets/ValidateDevelopmentCastingContainer.py'
        Assert-FoundationFile $castingPython; Assert-FoundationFile $castingGate
        $castingDirectory=Join-Path $context.Directory 'CastingContainerGate'
        $null=New-Item -ItemType Directory -Path $castingDirectory
        $castingReport=Join-Path $castingDirectory 'CastingContainer.json'
        $castingResult=Invoke-FoundationProcess -FilePath $castingPython -Arguments @($castingGate,'--csv',$csv,'--report',$castingReport) -WorkingDirectory $context.Workspace -OutputDirectory $castingDirectory -TimeoutSeconds 60
        $details.CastingContainerReport=$castingReport; $details.CastingContainerResult=$castingResult
        $code=$castingResult.ExitCode
    }
    if ($code -eq 0) {
        # 草地由客户端C++软路径加载：要求三张真实ExportBundleData，不能只凭编辑器保存或BulkData放行。
        $grassPython=Join-Path $engine.Root 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
        $grassGate=Join-Path $context.Workspace 'Tests/Assets/ValidateVillageGrassContainer.py'
        Assert-FoundationFile $grassPython; Assert-FoundationFile $grassGate
        $grassDirectory=Join-Path $context.Directory 'GrassContainerGate'
        $null=New-Item -ItemType Directory -Path $grassDirectory
        $grassReport=Join-Path $grassDirectory 'GrassContainer.json'
        $grassResult=Invoke-FoundationProcess -FilePath $grassPython -Arguments @($grassGate,'--csv',$csv,'--report',$grassReport) -WorkingDirectory $context.Workspace -OutputDirectory $grassDirectory -TimeoutSeconds 60
        $details.GrassContainerReport=$grassReport; $details.GrassContainerResult=$grassResult
        $code=$grassResult.ExitCode
    }
    $message='实际客户端Cook/Stage和IoStore资源门禁结束；仍须验证认证、UI交互与网络准入。'
} catch [IO.FileNotFoundException] { $code=2; $message=$_.Exception.Message }
catch { $code=1; $message=$_.Exception.Message }
finally { if ($lock) { $lock.ReleaseMutex(); $lock.Dispose() } }
if ($context) { Write-FoundationResult $context $code $message $details } else { Write-Host $message }
exit $code
