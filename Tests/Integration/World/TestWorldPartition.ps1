#requires -Version 7.0
<# Partition世界入口：调用者必须提供真实开发WP地图及定义；不创建伪umap或替换正式内容。
   宿主必须输出PartitionConfirmed、实际Source注册/Ready/撤销/清理证据，否则失败。 #>
param([switch]$Start,[string]$EngineRoot=$env:UE_ROOT,[string]$Map,[string]$DefinitionPackage,[string]$BuildResult,[ValidateRange(1,3600)][int]$TimeoutSeconds=120,[guid]$RunId=[guid]::NewGuid())
. (Join-Path $PSScriptRoot 'WorldValidationTools.ps1')
exit (Invoke-WorldRuntimeGate -Scenario Partition -RunId $RunId -Start:$Start -EngineRoot $EngineRoot -Map $Map -DefinitionPackage $DefinitionPackage -BuildResult $BuildResult -TimeoutSeconds $TimeoutSeconds)
