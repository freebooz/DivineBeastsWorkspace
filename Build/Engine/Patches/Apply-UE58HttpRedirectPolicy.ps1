<#
.SYNOPSIS
精确应用UE5.8请求级重定向补丁；检查版本、补丁摘要及每个目标的前后SHA256。
.DESCRIPTION
不依赖引擎Git仓库，不下载、不编译。已完整应用则幂等退出；混合状态或未知修改一律拒绝。
首次应用前备份到工作空间Saved。失败不自动回滚，避免覆盖并发修改；需人工核对备份。
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$EngineRoot,
    [switch]$CheckOnly,
    [string]$EvidenceRoot
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $EngineRoot).Path)
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $workspace 'Saved/Validation/EngineHttpRedirect' }
$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'UE5.8-HttpRedirectPolicy.hashes.json') -Raw | ConvertFrom-Json
$patch = Join-Path $PSScriptRoot 'UE5.8-HttpRedirectPolicy.patch'
if ((Get-FileHash -LiteralPath $patch -Algorithm SHA256).Hash -ne $manifest.PatchSHA256) { throw '补丁SHA256不匹配，拒绝应用。' }
$version = Get-Content -LiteralPath (Join-Path $root 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8 -or $version.PatchVersion -ne 0) { throw '只支持现场锁定UE5.8.0。' }
$allowed = @(
    'Engine/Source/Runtime/Online/HTTP/Public/Interfaces/IHttpRequest.h',
    'Engine/Source/Runtime/Online/HTTP/Private/Curl/CurlHttp.h',
    'Engine/Source/Runtime/Online/HTTP/Private/Curl/CurlHttp.cpp',
    'Engine/Source/Runtime/Online/HTTP/Private/Tests/HttpRedirectPolicyTests.cpp'
)
if ($manifest.Files.Count -ne $allowed.Count) { throw '目标文件清单数量不匹配。' }
$beforeCount = 0
$afterCount = 0
$seen = @{}
foreach ($file in $manifest.Files) {
    if ($file.Path -cnotin $allowed -or $seen.ContainsKey($file.Path)) { throw '清单含未授权或重复路径。' }
    $seen[$file.Path] = $true
    $target = Join-Path $root $file.Path
    # 禁止目标下的符号链接/联接把写入重定向到引擎之外。
    $ancestor = $target
    while ($ancestor -and $ancestor -ne $root) {
        if (Test-Path -LiteralPath $ancestor) {
            if ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "目标含重解析点，拒绝写入：$ancestor" }
        }
        $ancestor = Split-Path $ancestor -Parent
    }
    $hash = if (Test-Path -LiteralPath $target) { (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash } else { $null }
    if ($hash -eq $file.AfterSHA256) { $afterCount++ }
    elseif ($hash -eq $file.BeforeSHA256) { $beforeCount++ }
    else { throw "目标有未知修改或缺失，保留不覆盖：$($file.Path)" }
}
if ($afterCount -eq $allowed.Count) { Write-Output 'AlreadyApplied：所有目标后SHA256匹配，未修改。'; return }
if ($beforeCount -ne $allowed.Count) { throw '目标处于混合状态；不猜测或覆盖部分修改。' }
$previousCeiling = $env:GIT_CEILING_DIRECTORIES
# 测试副本可能位于另一个Git工作树的子目录；禁止Git向父目录发现仓库后静默忽略补丁路径。
$env:GIT_CEILING_DIRECTORIES = Split-Path $root -Parent
Push-Location $root
try {
    & git -c core.autocrlf=false apply --check -- $patch
    if ($LASTEXITCODE -ne 0) { throw "git apply --check失败：$LASTEXITCODE" }
    if ($CheckOnly) { Write-Output 'Ready：原SHA256和补丁上下文全部匹配，未修改。'; return }
    $backup = Join-Path $EvidenceRoot ('Apply-' + [guid]::NewGuid().ToString('N'))
    foreach ($file in $manifest.Files) {
        if ($file.BeforeSHA256) {
            $destination = Join-Path $backup $file.Path
            New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
            Copy-Item -LiteralPath (Join-Path $root $file.Path) -Destination $destination
        }
    }
    & git -c core.autocrlf=false apply -- $patch
    if ($LASTEXITCODE -ne 0) { throw "补丁应用失败；保留现状，备份：$backup" }
    foreach ($file in $manifest.Files) {
        if ((Get-FileHash -LiteralPath (Join-Path $root $file.Path) -Algorithm SHA256).Hash -ne $file.AfterSHA256) {
            throw "应用后SHA256不匹配，停止且不自动覆盖；备份：$backup"
        }
    }
    Write-Output "Applied：全部后SHA256匹配；未编译引擎。备份：$backup"
} finally {
    Pop-Location
    $env:GIT_CEILING_DIRECTORIES = $previousCeiling
}
