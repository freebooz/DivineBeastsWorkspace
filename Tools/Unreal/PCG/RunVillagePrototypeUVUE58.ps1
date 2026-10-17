#requires -Version 5.1
<#
.SYNOPSIS
SSLM新手村带UV技术PCG网格真实UE5.8导入与独立冷重开。
.DESCRIPTION
限定现有六份不可变Saved/Validation阶段OBJ，仅用于Development/PCG/MeshesUV。
输入必须通过哈希校验；绝不覆盖先前导入失败的六份旧技术网格、正式地图或美术。
检查本次UE独占进程、真实日志成功标记、Python错误、Interchange Ensure和原始输入SHA。
.PARAMETER EngineRoot
锁定UE5.8路径。
.PARAMETER Mode
Apply首次导入六份；Verify只读加载检查全部六份，不能以静态文件头冒充成功。
#>
[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE_ROOT,
    [ValidateSet('Apply','Verify')][string]$Mode = 'Verify'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$project = Join-Path $root 'Game/DivineBeastsArena.uproject'
$script = Join-Path $PSScriptRoot 'ImportVillagePrototypeUVUE58.py'
$stage = Join-Path $root 'Saved/Validation/GamePlatformPCG/TechnicalUVStageV1'
$run = Join-Path $root ('Saved/Validation/GamePlatformPCG/TechUVImport/' + [guid]::NewGuid().ToString('N'))
New-Item -Path $run -ItemType Directory -Force | Out-Null
try {
    $python = 'D:/UnrealEngine-5.8.0-release/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
    if (-not (Test-Path $python -PathType Leaf)) {throw '找不到锁定引擎自带Python3'}
    & $python (Join-Path $PSScriptRoot 'StageVillagePrototypeUV.py') --verify
    if ($LASTEXITCODE -ne 0) {throw 'UV源生成与固定SHA-256不同，拒绝启动UE导入'}
    $config = Get-Content (Join-Path $root 'Game/Config/DefaultGame.ini') -Raw -Encoding UTF8
    if (!$config.Contains('+DirectoriesToNeverCook=(Path="/DBAWorldPack_Village/Development/PCG")')) {
        throw '项目缺少Development PCG隔离目录的正式NeverCook配置'
    }
    if (-not $EngineRoot) {throw '必须指定UE5.8引擎'}
    $engine = (Resolve-Path -LiteralPath $EngineRoot).Path
    $version = Get-Content -Raw -Encoding UTF8 (Join-Path $engine 'Engine/Build/Build.version')|ConvertFrom-Json
    if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8) {throw '只允许UE5.8'}
    $editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    foreach ($file in @($editor,$project,$script)) {if (!(Test-Path $file -PathType Leaf)) {throw "缺少正式执行入口：$file"}}
    if (@(Get-Process UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool -ErrorAction SilentlyContinue).Count) {
        throw '当前UE Editor或UBT占用同一工作空间，禁止争用'
    }
    $ubt = @(Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" |
        Where-Object { $_.CommandLine -match 'UnrealBuildTool' })
    if ($ubt.Count) {throw '其他会话UE构建正在运行'}
    $hashes = @{}
    foreach ($file in @(Get-ChildItem -LiteralPath $stage -File -Filter '*.obj')) {
        $hashes[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
    if ($hashes.Count -ne 6) {throw 'UV阶段源六种OBJ不完整'}
    $stdout = Join-Path $run 'UnrealConsole.log'
    $stderr = Join-Path $run 'UnrealStderr.log'
    $unrealLog = Join-Path $run 'UnrealNative.log'
    $env:SSLM_VILLAGE_UV_TECH_IMPORT_MODE = $Mode.ToLowerInvariant()
    try {
        $args = @($project,('/Engine/Maps/Entry'),("-ExecutePythonScript=$script"),
            '-VillagePCGTechUV','-EnablePlugins=PythonScriptPlugin,DBAWorldPack_Village',
            '-DisablePlugins=Monolith','-NoCompile','-NoCompileEditor',
            '-unattended','-nop4','-nosplash','-nosound','-nullrhi',
            ("-abslog=$unrealLog"))
        $process = Start-Process -FilePath $editor -ArgumentList $args -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        if (!$process.WaitForExit(420000)) {
            $process.Kill()
            [void]$process.WaitForExit(5000)
            throw '本次UE独立导入超过420秒，仅终止本次自有进程'
        }
        $process.Refresh()
    } finally {
        Remove-Item Env:SSLM_VILLAGE_UV_TECH_IMPORT_MODE -ErrorAction SilentlyContinue
    }
    foreach ($item in $hashes.GetEnumerator()) {
        if ((Get-FileHash $item.Key -Algorithm SHA256).Hash -ne $item.Value) {
            throw "原始UV源被UE编辑器写入修改，拒绝验收：$($item.Key)"
        }
    }
    $log = if (Test-Path -LiteralPath $unrealLog -PathType Leaf) {
        Get-Content -LiteralPath $unrealLog -Raw -Encoding UTF8
    } else { '' }
    $pass = $log.Contains('SSLM_VILLAGE_UV_IMPORT_PASS 6')
    $errors = $log.Contains('LogPython: Error:') -or $log.Contains('Fatal error:') -or
        $log.Contains('Ensure condition failed: UVs.IsValidIndex') -or
        $log.Contains('EnsureFailed: Error:')
    Write-Output "SSLM_VILLAGE_UV_UE_RESULT mode=$Mode exit=$($process.ExitCode) marker=$pass errors=$errors"
    if ($process.ExitCode -ne 0 -or !$pass -or $errors) {
        throw "正式UE5.8没有安全完成六份UV技术网格：$unrealLog"
    }
    Write-Output 'SSLM_VILLAGE_UV_NATIVE_VERIFIED_ONLY_DEVELOPMENT_NEVER_COOK'
    exit 0
} catch {
    Write-Output ('SSLM_VILLAGE_UV_IMPORT_FAILED '+$_.Exception.Message)
    exit 1
} finally {
    Write-Output ('SSLM_VILLAGE_UV_IMPORT_EVIDENCE '+$run)
}
