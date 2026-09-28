# 《神兽联盟》输入架构静态门禁。
# 只检查源码分层、依赖和性能契约，不冒充UE运行、设备真机或网络技能验收。
$ErrorActionPreference = 'Stop'
$PluginRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$SourceRoot = Join-Path $PluginRoot 'Source\DivineBeastsInputClient'
$Utf8 = [System.Text.Encoding]::UTF8
$Files = Get-ChildItem $SourceRoot -Recurse -File -Include *.h,*.cpp,*.cs
$Text = ($Files | ForEach-Object { [System.IO.File]::ReadAllText($_.FullName, $Utf8) }) -join "`n"

# 项目层不得复制平台输入服务/EnhancedInput子系统，也不得引入业务级Tick。
$Forbidden = @(
    'class UDivineBeastsInputLocalPlayerSubsystem',
    'FTSTicker',
    'PrimaryComponentTick',
    'TickComponent(',
    'void Tick('
)
foreach($Token in $Forbidden){
    if($Text.Contains($Token)){ throw "Forbidden project input implementation detected: $Token" }
}

$Required = @(
    'IGamePlatformInputService',
    'ActivateGameplayInput(',
    'BindInputReceiver(',
    'DivineBeastsInputSemantics::ToAbilityInputTag',
    'UGamePlatformAbilitySystemComponent'
)
foreach($Token in $Required){
    if(-not $Text.Contains($Token)){ throw "Required project input integration missing: $Token" }
}

# 运行时项目桥不得重新使用平台旧Attack/AbilitySlot/TargetLock兼容枚举；
# 唯一允许引用旧枚举的位置是项目Profile校验，用于显式拒绝旧资产继续沿用。
$RuntimeBridgeFiles = @(
    (Join-Path $SourceRoot 'Private\Subsystems\DivineBeastsInputClientSubsystem.cpp'),
    (Join-Path $SourceRoot 'Private\Input\DivineBeastsInputSemantics.cpp')
)
$RuntimeBridgeText = ($RuntimeBridgeFiles | ForEach-Object { [System.IO.File]::ReadAllText($_, $Utf8) }) -join "`n"
$LegacyGameplayTokens = @(
    'EGamePlatformInputSemantic::AttackPrimary',
    'EGamePlatformInputSemantic::AbilitySlot1',
    'EGamePlatformInputSemantic::AbilitySlot2',
    'EGamePlatformInputSemantic::AbilitySlot3',
    'EGamePlatformInputSemantic::AbilitySlot4',
    'EGamePlatformInputSemantic::TargetLock',
    'GetSemanticTag(EGamePlatformInputSemantic::Move)',
    'GetSemanticTag(EGamePlatformInputSemantic::LookDelta)',
    'GetSemanticTag(EGamePlatformInputSemantic::LookRate)'
)
foreach($Token in $LegacyGameplayTokens){
    if($RuntimeBridgeText.Contains($Token)){ throw "Legacy platform semantic leaked back into project runtime bridge: $Token" }
}
if(-not $RuntimeBridgeText.Contains('GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::Move)')){
    throw 'Project runtime bridge must consume the platform Built-in semantic API.'
}

$Build = [System.IO.File]::ReadAllText((Join-Path $SourceRoot 'DivineBeastsInputClient.Build.cs'), $Utf8)
if(-not $Build.Contains('"GamePlatformInputClient"')){ throw 'GamePlatformInputClient dependency missing.' }
if(-not $Build.Contains('"GamePlatformAbilitySystem"')){ throw 'GamePlatformAbilitySystem dependency missing.' }

Write-Host 'DivineBeasts input architecture gate passed: platform-input=yes ability-bridge=yes business-tick=0.'
