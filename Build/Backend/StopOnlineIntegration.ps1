#Requires -Version 7.0
<#
仅停止本轮精确ID+name+labels匹配的容器；默认保留容器、网络、数据库卷以便恢复。
-RemoveContainers仅移除已停止的本轮容器和空网络，永不删除数据库卷。
-RemoveSecrets只删除本轮已知短期秘密文件；使用前应完成UE验收。移除后不能再用旧凭据测试。
停止前写入secret-free证据；不输出容器原始日志，不调用compose down/prune/remove-orphans。
#>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunId, [switch]$RemoveContainers, [switch]$RemoveSecrets)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'OnlineIntegrationTools.psm1') -Force
$state=Read-OnlineState $RunId
$cases=[Collections.Generic.List[object]]::new()
$path=Write-OnlineResult $state 'StopResult.json' 'NotExecuted' 0 @() @('PG卷及既有测试数据保留；证据写于停止之前。')
try {
    foreach ($role in @('gateway','player','identity','seed','pgsuite','build','postgres')) {
        $records=@($state.Resources | Where-Object { $_.Kind -eq 'container' -and $_.Role -eq $role -and -not $_.Removed })
        foreach ($entry in $records) {
            $record=Get-OnlineOwnedRecord $state $role
            $null=Invoke-OnlineDocker -Arguments @('stop','--time','15',$record.Id) -TimeoutSeconds 45
            if ($RemoveContainers) {
                $null=Get-OnlineOwnedRecord $state $role
                $null=Invoke-OnlineDocker -Arguments @('container','rm',$record.Id)
                $record.Removed=$true; Save-OnlineState $state
            }
            $cases.Add(@{Name="StopOwned_$role";Status='Passed'})
        }
    }
    if ($RemoveContainers) {
        foreach ($entry in @($state.Resources | Where-Object { $_.Kind -eq 'network' -and -not $_.Removed })) {
            $network=Get-OnlineOwnedRecord $state $entry.Role
            $null=Invoke-OnlineDocker -Arguments @('network','rm',$network.Id)
            $network.Removed=$true; Save-OnlineState $state
        }
    }
    if ($RemoveSecrets) {
        $root=Join-Path (Get-OnlineRunDirectory $RunId) 'Secrets'
        foreach ($name in @('credentials.json','database.env','postgres-password','pgtests.env')) {
            $secretPath=Assert-OnlineChildPath $root (Join-Path $root $name)
            if (Test-Path -LiteralPath $secretPath) { Assert-OnlinePrivateFile $secretPath; Remove-Item -LiteralPath $secretPath }
        }
        $cases.Add(@{Name='RemoveOwnedShortLivedSecrets';Status='Passed';Detail='精确删除四个已知文件，未递归删除目录或数据库卷。'})
    }
    $state.Status='Stopped'; Save-OnlineState $state
    $path=Write-OnlineResult $state 'StopResult.json' 'Passed' 0 $cases.ToArray() @('数据库卷永不由本脚本删除；未移除容器时可按清单ID重新启动；移除容器后恢复须重新核对卷归属并另行装配。')
    Write-Output "RunId=$RunId Status=Passed Result=$path DatabaseVolumeRetained=true"
} catch {
    $cases.Add(@{Name='OwnershipOrStop';Status='Failed'})
    $path=Write-OnlineResult $state 'StopResult.json' 'Failed' 1 $cases.ToArray()
    throw "本轮停止未完全完成，拒绝越过归属检查。RunId=$RunId Result=$path"
}
