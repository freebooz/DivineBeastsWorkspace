<#
.SYNOPSIS
编译并运行Core生产算法原生测试；不启动UE、不处理其他插件或占位描述文件。
.PARAMETER BuildDirectory
可选生成目录；默认由脚本实际位置解析到工作空间Saved/Validation/FoundationM0/CoreNative。
.PARAMETER Configuration
MSVC配置，默认Debug；失败传播原始工具退出码，日志与缓存都留在生成目录。
#>
[CmdletBinding()]
param(
    [string]$BuildDirectory,
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug'
)
$ErrorActionPreference = 'Stop'
$workspaceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../../..'))
if (-not (Test-Path -LiteralPath (Join-Path $workspaceRoot 'AGENTS.md'))) {
    throw '无法从Core插件位置定位工作空间根目录。'
}
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $workspaceRoot 'Saved/Validation/FoundationM0/CoreNative'
}
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null
$logPath = Join-Path $BuildDirectory ("Native-{0}-{1}.log" -f $Configuration, (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
Start-Transcript -LiteralPath $logPath | Out-Null
try {
    Write-Output "原生验证：$Configuration；生成目录：$BuildDirectory"
    & cmake --version
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & cmake -S $PSScriptRoot -B $BuildDirectory -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & cmake --build $BuildDirectory --config $Configuration
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
    exit $LASTEXITCODE
}
finally {
    Stop-Transcript | Out-Null
}
