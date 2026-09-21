param(
    [Parameter(Mandatory = $true)][string]$EngineRoot,
    [Parameter(Mandatory = $true)][string]$Project,
    [Parameter(Mandatory = $true)][string]$EditorTarget,
    [Parameter(Mandatory = $true)][string]$ReportDirectory
)

# 对已经启用本插件的真实宿主执行 UE5.8 构建与 Automation；不新建或复制正式插件源码。
$ErrorActionPreference = 'Stop'
$engineDirectory = Join-Path $EngineRoot 'Engine'
$buildScript = Join-Path $engineDirectory 'Build/BatchFiles/Build.bat'
$editorCommand = Join-Path $engineDirectory 'Binaries/Win64/UnrealEditor-Cmd.exe'
$versionFile = Join-Path $engineDirectory 'Build/Build.version'
foreach ($required in @($buildScript, $editorCommand, $versionFile, $Project)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "必需文件不存在：$required" }
}
$version = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8) { throw '必须使用锁定的 UE5.8 引擎；不能静默降低版本。' }
$projectFile = (Resolve-Path -LiteralPath $Project).Path
$projectDescriptor = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
if (-not ($projectDescriptor.Plugins | Where-Object { $_.Name -eq 'GamePlatformApplicationFlow' -and $_.Enabled })) {
    throw '宿主必须显式启用 GamePlatformApplicationFlow。'
}
# 唯一新报告目录防止把历史成功文件当成本次通过证据。
if (Test-Path -LiteralPath $ReportDirectory) { throw '报告目录已存在，请为本轮验证指定新目录。' }
& $buildScript $EditorTarget Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$reportRoot = [IO.Path]::GetFullPath($ReportDirectory)
& $editorCommand $projectFile -unattended -nop4 -NullRHI -nosplash `
    '-ExecCmds=Automation RunTests GamePlatform.ApplicationFlow' `
    '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportRoot" -stdout -FullStdOutLogOutput
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$indexPath = Join-Path $reportRoot 'index.json'
if (-not (Test-Path -LiteralPath $indexPath)) { throw 'UE 未生成 Automation 报告，不能认定通过。' }
$report = Get-Content -LiteralPath $indexPath -Raw | ConvertFrom-Json
$cases = @($report.tests | Where-Object { $_.fullTestPath -like 'GamePlatform.ApplicationFlow.*' })
if ($cases.Count -lt 28 -or @($cases | Where-Object { $_.state -ne 'Success' }).Count -ne 0) {
    throw "流程测试缺失或失败：实际发现 $($cases.Count) 项，要求至少 28 项全部成功。"
}
Write-Output "UE流程测试通过：$($cases.Count) 项；报告：$indexPath"
exit 0
