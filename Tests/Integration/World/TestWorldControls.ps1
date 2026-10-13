# 世界PC操控静态门禁：检查真实键位冲突、双端隔离和生命周期接线；可运行已编译纯策略测试。
# 输入PolicyExecutable是当前源码编译产物，空值仅执行静态检查；不启动编辑器/客户端，不修改资源或用户配置。
# 失败抛异常或传播测试进程退出码；通过不代表Slate、Cook、网络朝向或人工视觉验收。
param([string]$PolicyExecutable)
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$Utf8 = [System.Text.Encoding]::UTF8
$Input = [System.IO.File]::ReadAllText((Join-Path $Root 'Game/Config/DefaultInput.ini'), $Utf8)
$Controller = [System.IO.File]::ReadAllText((Join-Path $Root 'Game/Plugins/DivineBeasts/DBAWorlds/Source/DBAWorldsRuntime/Private/Gameplay/DivineBeastsWorldPlayerController.cpp'), $Utf8)
$Build = [System.IO.File]::ReadAllText((Join-Path $Root 'Game/Plugins/DivineBeasts/DBAWorlds/Source/DBAWorldsRuntime/DBAWorldsRuntime.Build.cs'), $Utf8)

# 世界转身/横移键不允许同时授予技能，重复键会触发移动与技能两个业务命令。
foreach ($Line in ($Input -split "`r?`n")) {
    if ($Line -match '^\+ActionMappings=.*ActionName="DBA\.(PrimaryAbility|AbilityOne|AbilityTwo|UltimateAbility)".*Key=(W|S|A|D|Q|E|LeftMouseButton|RightMouseButton)[),]') {
        throw "Gameplay skill conflicts with world movement: $Line"
    }
}
foreach ($Binding in @(
    'AxisName="DBA.KeyboardTurn",Scale=1.000000,Key=D',
    'AxisName="DBA.KeyboardTurn",Scale=-1.000000,Key=A',
    'AxisName="DBA.MoveRight",Scale=1.000000,Key=E',
    'AxisName="DBA.MoveRight",Scale=-1.000000,Key=Q',
    'AxisName="DBA.CameraZoom",Scale=1.000000,Key=MouseWheelAxis',
    'ActionName="DBA.CameraObserve"', 'ActionName="DBA.CameraSteer"', 'Key=Escape',
    'ActionName="DBA.Inventory"', 'Key=I',
    'DefaultViewportMouseCaptureMode=NoCapture', 'bShouldFlushPressedKeysOnViewportFocusLost=True')) {
    if (-not $Input.Contains($Binding)) { throw "World input configuration is missing: $Binding" }
}
foreach ($Hook in @('::FlushPressedKeys()', '::EndPlay(', '::PreClientTravel(', '::OnUnPossess()', '::SetWorldGameplayInputSuspended(',
    'OnWorldEscapeRequested.Broadcast()', 'ConsumeMovementInputVector()', 'StopJumping()',
    'OnWorldInventoryRequested.Broadcast()',
    'ClearAbilityInput()', 'SetMouseLocation(', 'bDoCollisionTest = true')) {
    if (-not $Controller.Contains($Hook)) { throw "World controls cleanup/integration is missing: $Hook" }
}
# 背包按键由世界控制器只发布中立本地委托，快捷键不能复制为二套权威库存请求。
$InventoryBindings = [regex]::Matches($Input, '(?m)^\+ActionMappings=\(ActionName="DBA\.Inventory"[^\r\n]*Key=I\)')
if ($InventoryBindings.Count -ne 1) {
    throw '背包I键必须有且只有一个准确原生ActionMapping。'
}
if ($Build.Contains('DivineBeastsUIClient') -or $Controller.Contains('DivineBeastsUIClientSubsystem')) {
    throw 'Shared world runtime must not depend on client UI implementation.'
}
# 未处理的按钮右键/禁用按钮左键仍是UI；真实Slate路径必须忽略Enabled过滤后检查实际SViewport叶子。
foreach ($Hook in @('LocateWindowUnderMouse(', 'GetInteractiveTopLevelWindows(), true, UserIndex',
    'Path.ContainsWidget(GameViewport.Get())', 'Path.GetLastWidget()', 'MouseButtonMask == 0 && !IsPointerOverWorldViewport()',
    'UpdateOwnedInputLock(', 'SetIgnoreMoveInput(MoveDelta > 0)', 'SetIgnoreLookInput(LookDelta > 0)', '#if !UE_SERVER')) {
    if (-not $Controller.Contains($Hook)) { throw "World pointer ownership or input lock ownership is missing: $Hook" }
}
if ($Controller.Contains('ResetIgnoreMoveInput(') -or $Controller.Contains('ResetIgnoreLookInput(')) {
    throw 'World controller must not clear input lock counts belonging to other systems.'
}
if (-not $Build.Contains('Target.Type != TargetType.Server') -or -not $Build.Contains('"Slate", "SlateCore"')) {
    throw 'Slate pointer hit testing must remain excluded from the dedicated server target.'
}
if ($PolicyExecutable) {
    if (-not (Test-Path -LiteralPath $PolicyExecutable -PathType Leaf)) { throw 'Policy executable does not exist.' }
    & $PolicyExecutable
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
Write-Host 'WORLD_CONTROLS_STATIC_PASS (not a viewport or network acceptance result)'
