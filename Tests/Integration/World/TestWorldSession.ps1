#requires -Version 7.0
<# Session世界入口：当前公开服务、真实快照和双进程适配缺失，明确返回NotExecuted 2。
   不读取Session私有状态，不伪造分配，不以Foundation开发上下文替代网络身份。 #>
param([switch]$Start,[string]$EngineRoot=$env:UE_ROOT,[ValidateRange(1,3600)][int]$TimeoutSeconds=120,[guid]$RunId=[guid]::NewGuid())
. (Join-Path $PSScriptRoot 'WorldValidationTools.ps1')
exit (Invoke-WorldRuntimeGate -Scenario Session -RunId $RunId -Start:$Start -EngineRoot $EngineRoot -TimeoutSeconds $TimeoutSeconds)
