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
function Invoke-NativeCheck {
    # 显式收集原生命令标准输出与错误；非交互式PowerShell转录可能遗漏原生输出。
    param([string]$Command, [string[]]$Arguments)
    Get-Command -Name $Command -CommandType Application -ErrorAction Stop | Out-Null
    "命令：$Command $($Arguments -join ' ')" | Tee-Object -FilePath $logPath -Append
    $savedErrorPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $Command @Arguments 2>&1 | Tee-Object -FilePath $logPath -Append
        $toolExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedErrorPreference
    }
    "退出码：$toolExitCode" | Tee-Object -FilePath $logPath -Append
    if ($toolExitCode -ne 0) { exit $toolExitCode }
}
"原生验证：$Configuration；生成目录：$BuildDirectory" | Tee-Object -FilePath $logPath -Append
Invoke-NativeCheck 'cmake' @('--version')
Invoke-NativeCheck 'cmake' @('-S', $PSScriptRoot, '-B', $BuildDirectory, '-G', 'Visual Studio 17 2022', '-A', 'x64')
Invoke-NativeCheck 'cmake' @('--build', $BuildDirectory, '--config', $Configuration)
Invoke-NativeCheck 'ctest' @('--test-dir', $BuildDirectory, '-C', $Configuration, '--output-on-failure')
exit 0
