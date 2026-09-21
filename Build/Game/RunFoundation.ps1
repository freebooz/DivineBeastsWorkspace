#requires -Version 7.0
<#
.SYNOPSIS
有界本地冒烟：必须-Start并显式-FoundationStandalone；默认要求FullM0完整就绪。
.DESCRIPTION
Target=Editor/Client/Server。Editor使用UnrealEditor.exe -game；Client/Server必须提供StageDirectory。
Phase=HostOnly仅接受FoundationHostReady；FullM0接受FoundationReady；Server始终只接受FoundationServerReady。
所有启动携带-FoundationRunId=<guid>。服务端仅回环绑定，无玩家流程。就绪后终止本次进程。
返回0满足所选阶段，1超时/未就绪，2未执行；外部非零码保留。HostOnly通过不等于M0通过。
#>
param([string]$EngineRoot, [switch]$Start, [switch]$FoundationStandalone,
    [ValidateSet('Editor','Client','Server')][string]$Target = 'Editor',
    [ValidateSet('FullM0','HostOnly')][string]$Phase = 'FullM0', [string]$StageDirectory,
    [string]$Map = '/Game/Development/Foundation/Maps/L_FoundationBootstrap',
    [ValidateRange(1,65535)][int]$Port = 17777,
    [ValidateRange(1,86400)][int]$TimeoutSeconds = 120, [switch]$NullRHI,
    [guid]$RunId = [guid]::NewGuid())
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
$context = $null; $code = 2; $message = ''; $details = @{ Target=$Target; Phase=$Phase }
try {
    $context = New-FoundationContext "Run-$Target" $RunId
    if (-not $Start -or -not $FoundationStandalone) { throw [IO.FileNotFoundException]::new('需显式指定-Start -FoundationStandalone。') }
    $engine = Get-FoundationEngine $EngineRoot
    $details.EngineVersion = $engine.Version; $details.EngineRoot = $engine.Root
    Assert-FoundationFile $context.Project
    Assert-FoundationMaps $context @($Map)
    $arguments = @()
    if ($Target -eq 'Editor') {
        $executable = Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor.exe'
        $arguments += @($context.Project,$Map,'-game')
    } else {
        if (-not $StageDirectory -or -not (Test-Path -LiteralPath $StageDirectory -PathType Container)) { throw [IO.FileNotFoundException]::new('客户端/服务器运行需已有-StageDirectory。') }
        $matches = @(Get-ChildItem -LiteralPath $StageDirectory -Recurse -File -Filter "DivineBeastsArena$Target.exe" | Where-Object { $_.Directory.Name -eq 'Win64' })
        if ($matches.Count -ne 1) { throw [IO.FileNotFoundException]::new('Stage必须包含唯一的Win64目标程序。') }
        $executable = $matches[0].FullName
        $arguments += $Map
    }
    Assert-FoundationFile $executable
    $log = Join-Path $context.Directory 'Unreal.log'
    $arguments += @('-FoundationStandalone',"-FoundationRunId=$($context.RunId)",'-unattended','-nosplash','-nosound','-stdout','-FullStdOutLogOutput','-UTF8Output',"-abslog=$log",'-NoLiveCoding','-NoHotReload','-Messaging=0','-UDPMESSAGING_TRANSPORT_ENABLE=0','-TCPMESSAGING_TRANSPORT_ENABLE=0','-MULTIHOME=127.0.0.1')
    $arguments += '-CustomConfig=FoundationStandalone'
    if ($NullRHI) { $arguments += '-nullrhi' }
    $marker = if ($Phase -eq 'HostOnly') { 'FoundationHostReady' } else { 'FoundationReady' }
    if ($Target -eq 'Server') { $marker = 'FoundationServerReady'; $arguments += @('-server',"-port=$Port",'-QueryPort=0','-nosteam') }
    $details.ExpectedMarker = "$marker RunId=$($context.RunId)"; $details.Log = $log; $details.NullRHI = [bool]$NullRHI
    $result = Invoke-FoundationProcess -FilePath $executable -Arguments $arguments -WorkingDirectory (Split-Path $executable) -OutputDirectory $context.Directory -TimeoutSeconds $TimeoutSeconds -ReadyLog $log -ReadyMarker $details.ExpectedMarker
    $code = $result.ExitCode; $details.Result = $result
    $message = '只核验本次独占进程和本次RunId日志；不替代图形、联机或人工验收。'
} catch [IO.FileNotFoundException] { $code = 2; $message = $_.Exception.Message }
catch { $code = 1; $message = $_.Exception.Message }
$status = if ($details.ContainsKey('Result') -and $details.Result.ExitCode -ne 0) { 'Failed' } else { '' }
if ($context) { Write-FoundationResult $context $code $message $details $status } else { Write-Host $message }
exit $code
