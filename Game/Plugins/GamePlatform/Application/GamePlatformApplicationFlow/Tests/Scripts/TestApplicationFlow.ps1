param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$BuildDirectory,
    [string]$Generator = 'Visual Studio 17 2022'
)

# 编译并执行真实生产调度核心。产物默认进入工作空间 Saved，不写入插件源码。
$ErrorActionPreference = 'Stop'
$pluginRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
if (-not $BuildDirectory) {
    $BuildDirectory = [IO.Path]::GetFullPath((Join-Path $pluginRoot '../../../../../Saved/Validation/ApplicationFlow/Native'))
}
$cmakeCommand = (Get-Command cmake -ErrorAction Stop).Source
$ctestCommand = (Get-Command ctest -ErrorAction Stop).Source
& $cmakeCommand -S (Join-Path $pluginRoot 'Tests') -B $BuildDirectory -G $Generator -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmakeCommand --build $BuildDirectory --config $Configuration
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $ctestCommand --test-dir $BuildDirectory -C $Configuration --output-on-failure -V
exit $LASTEXITCODE
