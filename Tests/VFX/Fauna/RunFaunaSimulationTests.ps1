#requires -Version 7.0
<#
.SYNOPSIS
直接编译并运行平台群落正式 C++17 源码的 Windows 原生回归。
.DESCRIPTION
从脚本自身位置解析工程，使用已安装 Visual C++ 原生 x64 编译器。
仅在 Saved/Validation/FaunaNative/<RunId> 写编译响应文件、二进制与原始日志。
不复制生产算法、不加载 UE、不伪造引擎头；缺工具、编译失败与用例失败均传播退出码。
.PARAMETER RunId
证据目录名，只允许 ASCII 字母、数字、连字符与下划线；默认使用 UTC 时间和随机尾码。
#>
param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$RunId = ('Fauna_' + [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss') + '_' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$outputRoot = Join-Path $projectRoot ('Saved/Validation/FaunaNative/' + $RunId)
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$sourceRoot = Join-Path $projectRoot 'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Environment/Fauna'
$sourcePath = Join-Path $sourceRoot 'GamePlatformFaunaSimulation.cpp'
$testPath = Join-Path $PSScriptRoot 'FaunaSimulationTests.cpp'
$vswherePath = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath)) { throw '未找到 VS 安装发现工具；不能声称原生编译已执行。' }
$installationPath = (& $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if (-not $installationPath) { throw '未找到支持 x64 的 Visual C++ 工具链。' }
$vcvarsPath = Join-Path $installationPath 'VC/Auxiliary/Build/vcvars64.bat'
$compilerVersion = (Get-Content -LiteralPath (Join-Path $installationPath 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
$compilerPath = Join-Path $installationPath "VC/Tools/MSVC/$compilerVersion/bin/Hostx64/x64/cl.exe"
if (-not (Test-Path -LiteralPath $compilerPath)) { throw '已发现的 Visual C++ 编译器文件不存在。' }

# 响应文件传递完整路径；构建目录仅为本次验证所有，不覆盖其他任务制品。
$binaryPath = Join-Path $outputRoot 'FaunaSimulationTests.exe'
$responsePath = Join-Path $outputRoot 'Compile.rsp'
$responseLines = @(
    '/nologo', '/std:c++17', '/EHsc', '/utf-8', '/W4', '/WX', '/O2',
    ('/I"' + $sourceRoot + '"'),
    ('/Fo"' + $outputRoot + '\\"'),
    ('/Fe"' + $binaryPath + '"'),
    ('"' + $testPath + '"'),
    ('"' + $sourcePath + '"')
)
Set-Content -LiteralPath $responsePath -Value $responseLines -Encoding utf8NoBOM
$batchPath = Join-Path $outputRoot 'Compile.cmd'
$batchLines = @(
    '@echo off',
    ('call "' + $vcvarsPath + '" >nul'),
    'if errorlevel 1 exit /b %errorlevel%',
    ('"' + $compilerPath + '" @"' + $responsePath + '"'),
    'exit /b %errorlevel%'
)
Set-Content -LiteralPath $batchPath -Value $batchLines -Encoding ascii
$compileLog = Join-Path $outputRoot 'Compile.log'
$nativeErrorPreferenceBefore = $PSNativeCommandUseErrorActionPreference
$PSNativeCommandUseErrorActionPreference = $false
try {
    & $env:ComSpec /d /c $batchPath 2>&1 | Tee-Object -FilePath $compileLog
    $compileExitCode = $LASTEXITCODE
    $testExitCode = $null
    if ($compileExitCode -eq 0) {
        & $binaryPath 2>&1 | Tee-Object -FilePath (Join-Path $outputRoot 'Tests.log')
        $testExitCode = $LASTEXITCODE
    }
    $result = [ordered]@{
        TimestampUtc = [DateTime]::UtcNow.ToString('o')
        Compiler = $compilerPath
        CompilerVersion = $compilerVersion
        ProductionSource = $sourcePath
        CompileExitCode = $compileExitCode
        TestExitCode = $testExitCode
        Scope = '真实生产标准C++算法；不覆盖UE/GPU/Cook/网络'
    }
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputRoot 'Result.json') -Encoding utf8NoBOM
    Write-Output ("Evidence: " + $outputRoot)
    if ($compileExitCode -ne 0) { exit $compileExitCode }
    exit $testExitCode
}
finally { $PSNativeCommandUseErrorActionPreference = $nativeErrorPreferenceBefore }
