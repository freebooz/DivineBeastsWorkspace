#requires -Version 7.0
<# Foundation世界入口：真实定义、地图、区域、Ready、清理及新代次重入；缺前置返回2。
   BuildResult指向VerifyWorld实际Editor构建结果。未提供Start不会启动任何进程。
   默认FullLifecycle带FoundationWorldExercise，要求bootstrap真实往返；显式Readiness仅到WorldReady仍未执行2。
   DefinitionPackage只做文件检查；宿主接收的定义身份固定为GamePlatformDefinition:foundation.world@1。 #>
param([switch]$Start,[string]$EngineRoot=$env:UE_ROOT,[string]$Map='/Game/Development/Foundation/Maps/L_FoundationSandbox',[string]$DefinitionPackage,[string]$BuildResult,[ValidateSet('Readiness','FullLifecycle')][string]$Phase='FullLifecycle',[ValidateRange(1,3600)][int]$TimeoutSeconds=120,[guid]$RunId=[guid]::NewGuid())
. (Join-Path $PSScriptRoot 'WorldValidationTools.ps1')
exit (Invoke-WorldRuntimeGate -Scenario Foundation -RunId $RunId -Start:$Start -EngineRoot $EngineRoot -Map $Map -DefinitionPackage $DefinitionPackage -BuildResult $BuildResult -TimeoutSeconds $TimeoutSeconds -Phase $Phase)
