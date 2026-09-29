param(
    [string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../../../../../..')).Path
)

$ErrorActionPreference = 'Stop'
# 将调用方传入的相对路径规范化，避免不同Runner当前目录导致Join-Path得到不可用路径。
if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) { throw 'WorkspaceRoot is required.' }
$WorkspaceRoot = (Resolve-Path -LiteralPath $WorkspaceRoot -ErrorAction Stop).Path
$pluginRoot = Join-Path $WorkspaceRoot 'Game/Plugins/GamePlatform/Application/GamePlatformSettings'
$errors = New-Object 'System.Collections.Generic.List[string]'
$warnings = New-Object 'System.Collections.Generic.List[string]'

function Add-Error([string]$Message) {
    $errors.Add($Message)
}

function Get-ModuleSourceText([string]$ModuleName) {
    $moduleRoot = Join-Path $pluginRoot ('Source/' + $ModuleName)
    if (-not (Test-Path -LiteralPath $moduleRoot -PathType Container)) {
        return ''
    }

    $chunks = @(
        Get-ChildItem -LiteralPath $moduleRoot -Recurse -File -Include '*.h','*.cpp','*.cs' |
            ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 }
    )
    return ($chunks -join [Environment]::NewLine)
}

# 中文说明：脚本保持Windows PowerShell 5.1兼容；错误字符串采用ASCII，中文规则写在注释中。
$descriptorPath = Join-Path $pluginRoot 'GamePlatformSettings.uplugin'
if (-not (Test-Path -LiteralPath $descriptorPath -PathType Leaf)) {
    Add-Error 'Missing GamePlatformSettings.uplugin.'
} else {
    try {
        $descriptor = Get-Content -LiteralPath $descriptorPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($descriptor.CanContainContent -ne $false) {
            Add-Error 'Settings must set CanContainContent=false.'
        }

        $expectedModules = @(
            @{ Name='GamePlatformSettingsRuntime'; Type='Runtime'; Targets=@('Client','Editor','Server') },
            @{ Name='GamePlatformSettingsClient'; Type='ClientOnly'; Targets=@('Client','Editor') },
            @{ Name='GamePlatformSettingsServer'; Type='ServerOnly'; Targets=@('Editor','Server') },
            @{ Name='GamePlatformSettingsEditor'; Type='Editor'; Targets=@('Editor') }
        )

        $modules = @($descriptor.Modules)
        if ($modules.Count -ne $expectedModules.Count) {
            Add-Error ('Settings must declare exactly four modules; actual={0}.' -f $modules.Count)
        }

        foreach ($expected in $expectedModules) {
            $actual = @($modules | Where-Object { $_.Name -eq $expected.Name })
            if ($actual.Count -ne 1) {
                Add-Error ('Missing or duplicate module: {0}' -f $expected.Name)
                continue
            }

            if ($actual[0].Type -ne $expected.Type) {
                Add-Error ('Module {0} type must be {1}.' -f $expected.Name,$expected.Type)
            }

            $actualTargets = @($actual[0].TargetAllowList | Sort-Object)
            $expectedTargets = @($expected.Targets | Sort-Object)
            if (($actualTargets -join '|') -ne ($expectedTargets -join '|')) {
                Add-Error ('Module {0} target allow list is invalid.' -f $expected.Name)
            }
        }

        $pluginDeps = @($descriptor.Plugins | ForEach-Object Name | Sort-Object)
        if (($pluginDeps -join '|') -ne 'GamePlatformCore') {
            Add-Error 'Settings plugin dependency must be GamePlatformCore only.'
        }
    } catch {
        Add-Error ("Descriptor parse failed: {0}" -f $_.Exception.Message)
    }
}

$required = @(
    'Source/GamePlatformSettingsRuntime/Public/Interfaces/IGamePlatformSettingsService.h',
    'Source/GamePlatformSettingsRuntime/Public/Interfaces/IGamePlatformSettingsProvider.h',
    'Source/GamePlatformSettingsRuntime/Public/Types/GamePlatformSettingTypes.h',
    'Source/GamePlatformSettingsRuntime/Public/Subsystems/GamePlatformSettingsSubsystem.h',
    'Source/GamePlatformSettingsRuntime/Private/Tests/GamePlatformSettingsRuntimeTests.cpp',
    'Source/GamePlatformSettingsClient/Public/Interfaces/IGamePlatformDeviceSettingsService.h',
    'Source/GamePlatformSettingsClient/Public/Types/GamePlatformDeviceSettingsTypes.h',
    'Source/GamePlatformSettingsClient/Private/Subsystems/GamePlatformDeviceSettingsSubsystem.h',
    'Source/GamePlatformSettingsClient/Private/Subsystems/GamePlatformDeviceSettingsSubsystem.cpp',
    'Source/GamePlatformSettingsClient/Private/Policy/GamePlatformDeviceSettingsPolicy.h',
    'Source/GamePlatformSettingsClient/Private/Policy/GamePlatformDeviceSettingsPolicy.cpp',
    'Source/GamePlatformSettingsClient/Private/Tests/GamePlatformDeviceSettingsPolicyTests.cpp',
    'Source/GamePlatformSettingsServer/Private/Server/GamePlatformSettingsServerPersistenceProvider.cpp',
    'Source/GamePlatformSettingsEditor/Private/Validation/GamePlatformSettingsEditorValidator.cpp',
    'Docs/Architecture.md',
    'Docs/API.md',
    'Docs/PerformanceAndSecurity.md',
    'Docs/TestingAndEvidence.md',
    'Docs/ManualReview.md'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $pluginRoot $relative) -PathType Leaf)) {
        Add-Error ("Missing required file: {0}" -f $relative)
    }
}

$runtimeText = Get-ModuleSourceText 'GamePlatformSettingsRuntime'
$runtimeBuildPath = Join-Path $pluginRoot 'Source/GamePlatformSettingsRuntime/GamePlatformSettingsRuntime.Build.cs'
if (Test-Path -LiteralPath $runtimeBuildPath) {
    $runtimeBuild = Get-Content -LiteralPath $runtimeBuildPath -Raw -Encoding UTF8
    foreach ($forbiddenDependency in @('MobaCommon','DivineBeasts','GamePlatformInput','GamePlatformUI','GamePlatformCamera','GamePlatformSFX','GamePlatformSave','GamePlatformData','GamePlatformOnline','GamePlatformSession','GamePlatformServer','GamePlatformTelemetry','GamePlatformPresentation')) {
        if ($runtimeBuild -match [regex]::Escape($forbiddenDependency)) {
            Add-Error ("Runtime Build.cs contains forbidden layer/domain dependency: {0}" -f $forbiddenDependency)
        }
    }
}
$runtimeIncludes = @(
    Get-ChildItem -LiteralPath (Join-Path $pluginRoot 'Source/GamePlatformSettingsRuntime') -Recurse -File -Include '*.h','*.cpp' |
        Select-String -Pattern '^\s*#include\s+"([^"]+)"'
)
foreach ($include in $runtimeIncludes) {
    if ($include.Line -match 'DivineBeasts|MobaCommon|GamePlatform(Input|UI|Camera|SFX|Save|Data)') {
        Add-Error ("Runtime include crosses settings boundary: {0}" -f $include.Line.Trim())
    }
}

$clientText = Get-ModuleSourceText 'GamePlatformSettingsClient'
$serverText = Get-ModuleSourceText 'GamePlatformSettingsServer'
$editorText = Get-ModuleSourceText 'GamePlatformSettingsEditor'
$allSourceText = @($runtimeText,$clientText,$serverText,$editorText) -join [Environment]::NewLine

# Runtime只能提供中立设置契约/解析基础，不得直接依赖具体领域实现或项目层。
foreach ($forbidden in @(
    'GamePlatformInputClient',
    'GamePlatformCameraClient',
    'GamePlatformSFXClient',
    'GamePlatformUIClient',
    'GamePlatformSaveClient',
    'GamePlatformData',
    'GamePlatformOnline',
    'GamePlatformSession',
    'GamePlatformServer',
    'GamePlatformTelemetry',
    'GamePlatformPresentation',
    'Niagara',
    'HttpModule'
)) {
    if ($runtimeText -match [regex]::Escape($forbidden)) {
        Add-Error ("Runtime contains forbidden domain/layer dependency: {0}" -f $forbidden)
    }
}

# Client可以依赖Runtime和引擎本地持久化，但不得直接拉入其他领域或Server/Editor实现。
foreach ($forbidden in @(
    'GamePlatformInputClient',
    'GamePlatformCameraClient',
    'GamePlatformSFXClient',
    'GamePlatformUIClient',
    'GamePlatformSaveClient',
    'GamePlatformSettingsServer',
    'GamePlatformSettingsEditor',
    'GamePlatformOnline',
    'GamePlatformSession',
    'GamePlatformServer',
    'GamePlatformTelemetry',
    'GamePlatformPresentation',
    'Niagara',
    'HttpModule'
)) {
    if ($clientText -match [regex]::Escape($forbidden)) {
        Add-Error ("Client contains forbidden horizontal/endpoint dependency: {0}" -f $forbidden)
    }
}

# Server允许读取GConfig/环境变量/命令行，但不得带入客户端表现或UGameUserSettings。
foreach ($forbidden in @(
    'GamePlatformSettingsClient',
    'GamePlatformSettingsEditor',
    'GamePlatformInputClient',
    'GamePlatformCameraClient',
    'GamePlatformSFXClient',
    'GamePlatformUIClient',
    'UGameUserSettings',
    'Niagara',
    'HttpModule'
)) {
    if ($serverText -match [regex]::Escape($forbidden)) {
        Add-Error ("Server contains forbidden client/domain dependency: {0}" -f $forbidden)
    }
}

# Editor只消费Runtime公开校验入口，不依赖Client/Server私有实现。
foreach ($forbidden in @(
    'GamePlatformSettingsClient',
    'GamePlatformSettingsServer',
    'GamePlatformInputClient',
    'GamePlatformCameraClient',
    'GamePlatformSFXClient',
    'GamePlatformUIClient'
)) {
    if ($editorText -match [regex]::Escape($forbidden)) {
        Add-Error ("Editor contains forbidden endpoint dependency: {0}" -f $forbidden)
    }
}

if ($allSourceText -match 'FTSTicker' -or
    $allSourceText -match 'AddTicker' -or
    $allSourceText -match '(?m)\bTick\s*\(') {
    Add-Error 'Settings must not depend on Tick/Ticker polling.'
}

$clientBuildPath = Join-Path $pluginRoot 'Source/GamePlatformSettingsClient/GamePlatformSettingsClient.Build.cs'
$clientPersistencePath = Join-Path $pluginRoot 'Source/GamePlatformSettingsClient/Private/Persistence/GamePlatformSettingsClientPersistenceProvider.cpp'
if (Test-Path -LiteralPath $clientPersistencePath) {
    $clientPersistence = Get-Content -LiteralPath $clientPersistencePath -Raw -Encoding UTF8
    if ($clientPersistence -notmatch 'FSHA1' -or
        $clientPersistence -notmatch 'GetScopedProfileSlotName' -or
        $clientPersistence -notmatch 'SettingsUserContextMissing') {
        Add-Error 'Client User Profile must isolate slots by opaque hashed user context and reject save without a context.'
    }
}

$runtimeServicePath = Join-Path $pluginRoot 'Source/GamePlatformSettingsRuntime/Public/Interfaces/IGamePlatformSettingsService.h'
if (Test-Path -LiteralPath $runtimeServicePath) {
    $runtimeService = Get-Content -LiteralPath $runtimeServicePath -Raw -Encoding UTF8
    if ($runtimeService -notmatch 'SwitchUserContext') {
        Add-Error 'Runtime service must expose a neutral user-context switch without depending on Online.'
    }
}

if (Test-Path -LiteralPath $clientBuildPath) {
    $build = Get-Content -LiteralPath $clientBuildPath -Raw -Encoding UTF8
    foreach ($requiredDependency in @('"Core"','"CoreUObject"','"Engine"','"GamePlatformCore"','"GamePlatformSettingsRuntime"')) {
        if ($build -notmatch [regex]::Escape($requiredDependency)) {
            Add-Error ("Client Build.cs missing required dependency: {0}" -f $requiredDependency)
        }
    }
}

$deviceSubsystemPath = Join-Path $pluginRoot 'Source/GamePlatformSettingsClient/Private/Subsystems/GamePlatformDeviceSettingsSubsystem.cpp'
if (Test-Path -LiteralPath $deviceSubsystemPath) {
    $subsystem = Get-Content -LiteralPath $deviceSubsystemPath -Raw -Encoding UTF8
    if ($subsystem -notmatch 'IsRunningDedicatedServer\(\)' -or
        $subsystem -notmatch 'IsRunningCommandlet\(\)') {
        Add-Error 'Device settings subsystem must reject dedicated server and commandlet environments.'
    }
    if ($subsystem -notmatch 'SaveSettings\(\)' -or
        $subsystem -notmatch 'ConfirmVideoMode\(\)') {
        Add-Error 'Device settings must provide explicit confirm and save paths.'
    }
    if ($subsystem -notmatch 'IsMutationEnvironmentAllowed\(\)' -or
        $subsystem -notmatch 'ValidateSettings\(\)') {
        Add-Error 'Device settings must guard disk validation behind the mutation-environment policy.'
    }
}

# Runtime异步保存期间的Provider拓扑变化必须延迟处理，避免Registry/Layers与保存代次竞态。
$runtimeSubsystemPath = Join-Path $pluginRoot 'Source/GamePlatformSettingsRuntime/Private/Subsystems/GamePlatformSettingsSubsystem.cpp'
if (Test-Path -LiteralPath $runtimeSubsystemPath) {
    $runtimeSubsystem = Get-Content -LiteralPath $runtimeSubsystemPath -Raw -Encoding UTF8
    if ($runtimeSubsystem -notmatch 'bPendingTopologyReload' -or
        $runtimeSubsystem -notmatch 'bSaveInFlight') {
        Add-Error 'Runtime must defer provider topology reload while async save is in flight.'
    }
}

# bSensitive只允许会话临时值；字符串设置必须有容量上限，Server不得把敏感值从INI/环境变量/命令行读入。
$validationPath = Join-Path $pluginRoot 'Source/GamePlatformSettingsRuntime/Private/Validation/GamePlatformSettingsValidation.cpp'
if (Test-Path -LiteralPath $validationPath) {
    $validationText = Get-Content -LiteralPath $validationPath -Raw -Encoding UTF8
    if ($validationText -notmatch 'SettingsSensitivePersistenceUnsupported' -or
        $validationText -notmatch 'SettingsStringTooLong') {
        Add-Error 'Runtime validation must enforce sensitive-persistence and string-capacity boundaries.'
    }
    if ($validationText -notmatch 'SettingsServerDefaultScopeInvalid' -or
        $validationText -notmatch 'SettingsServerScopeRuntimeInvalid') {
        Add-Error 'Runtime validation must prevent ServerDefault/server persistence from leaking into non-server runtime scopes.'
    }
}
$serverPersistencePath = Join-Path $pluginRoot 'Source/GamePlatformSettingsServer/Private/Server/GamePlatformSettingsServerPersistenceProvider.cpp'
if (Test-Path -LiteralPath $serverPersistencePath) {
    $serverPersistence = Get-Content -LiteralPath $serverPersistencePath -Raw -Encoding UTF8
    if ($serverPersistence -notmatch 'SettingsSensitiveServerOverrideUnsupported') {
        Add-Error 'Server persistence must reject sensitive INI/environment/command-line overrides.'
    }
    if ($serverPersistence -notmatch 'SettingsEnvironmentKeyCollision') {
        Add-Error 'Server persistence must reject normalized environment-key collisions.'
    }
    if ($serverPersistence -notmatch 'bHasServerDefault' -or
        $serverPersistence -notmatch 'bHasEnvironment' -or
        $serverPersistence -notmatch 'bHasCommandLine') {
        Add-Error 'Sensitive server settings must fail only on actual external override attempts, not merely on descriptor presence.'
    }
    if ($serverPersistence -notmatch 'SetUserContext' -or
        $serverPersistence -notmatch 'SettingsUserContextServerUnsupported') {
        Add-Error 'Server persistence must explicitly reject client User Profile contexts.'
    }
}

# 当前是否存在真正的生产Descriptor Provider是成熟度证据，不作为纯结构错误。
$providerImplementations = @(
    Get-ChildItem -LiteralPath (Join-Path $pluginRoot 'Source') -Recurse -File -Include '*.h','*.cpp' |
        Where-Object { $_.FullName -notmatch '[\\/]Tests[\\/]' } |
        Select-String -Pattern 'public\s+IGamePlatformSettingsProvider' -List
)
if ($providerImplementations.Count -eq 0) {
    $warnings.Add('No production IGamePlatformSettingsProvider implementation is registered; Runtime/Server remain framework-only.')
}

if ($errors.Count -gt 0) {
    Write-Output ("GamePlatformSettings architecture gate failed: {0}" -f $errors.Count)
    $errors | ForEach-Object { Write-Output ("  - {0}" -f $_) }
    if ($warnings.Count -gt 0) {
        $warnings | ForEach-Object { Write-Output ("  WARNING: {0}" -f $_) }
    }
    exit 1
}

Write-Output 'GamePlatformSettings architecture gate passed.'
$warnings | ForEach-Object { Write-Output ("WARNING: {0}" -f $_) }
exit 0

