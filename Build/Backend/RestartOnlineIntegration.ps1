#Requires -Version 7.0
<# 仅重启清单中的三个已创建服务；不重跑迁移、不重建容器、不操作数据库进程。用于证明资料跨服务重启持久化。 #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunId, [ValidateRange(10,300)][int]$TimeoutSeconds=90)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'OnlineIntegrationTools.psm1') -Force
$state=Read-OnlineState $RunId
$cases=[Collections.Generic.List[object]]::new()
try {
    if (-not $state.Migrated) { throw '本轮数据库迁移尚未成功。' }
    foreach ($role in @('identity','player','gateway')) {
        $record=Get-OnlineOwnedRecord $state $role
        $null=Invoke-OnlineDocker -Arguments @('restart','--time','15',$record.Id) -TimeoutSeconds 45
        $cases.Add(@{Name="RestartOwned_$role";Status='Passed'})
    }
    Wait-OnlineProbe $state $TimeoutSeconds
    $cases.Add(@{Name='RealProbeAfterRestart';Status='Passed'})
    $path=Write-OnlineResult $state 'RestartResult.json' 'Passed' 0 $cases.ToArray()
    Write-Output "RunId=$RunId Status=Passed Result=$path"
} catch {
    $cases.Add(@{Name='RestartOrProbe';Status='Failed'})
    $path=Write-OnlineResult $state 'RestartResult.json' 'Failed' 1 $cases.ToArray()
    throw "本轮重启失败。RunId=$RunId Result=$path"
}
