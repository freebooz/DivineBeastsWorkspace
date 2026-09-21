#requires -Version 7.0
<# Foundation世界入口：真实定义、地图、区域、Ready、清理及新代次重入；缺前置返回2。
   BuildResult指向VerifyWorld实际Editor构建结果。未提供Start不会启动任何进程。
   默认Readiness只验证到WorldReady，完整生命周期仍未执行2；FullLifecycle需要真实旅行驱动，脚本不造事件。 #>
param([switch]$Start,[string]$EngineRoot=$env:UE_ROOT,[string]$Map='/Game/Development/Foundation/Maps/L_FoundationSandbox',[string]$DefinitionPackage,[string]$BuildResult,[ValidateSet('Readiness','FullLifecycle')][string]$Phase='Readiness',[ValidateRange(1,3600)][int]$TimeoutSeconds=120,[guid]$RunId=[guid]::NewGuid())
. (Join-Path $PSScriptRoot 'WorldValidationTools.ps1')
exit (Invoke-WorldRuntimeGate -Scenario Foundation -RunId $RunId -Start:$Start -EngineRoot $EngineRoot -Map $Map -DefinitionPackage $DefinitionPackage -BuildResult $BuildResult -TimeoutSeconds $TimeoutSeconds -Phase $Phase)
