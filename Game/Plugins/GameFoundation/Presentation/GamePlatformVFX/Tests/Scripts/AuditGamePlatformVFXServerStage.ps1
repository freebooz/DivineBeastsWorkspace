param(
    [Parameter(Mandatory=$true)][string]$StageDirectory
)

$Patterns = @(
    "GamePlatformVFXClient",
    "FXS_GPVFX_",
    "FXE_GPVFX_",
    "FXM_GPVFX_",
    "L_GPVFX_Review"
)

$Hits = @()
foreach ($Pattern in $Patterns) {
    $Found = Get-ChildItem -Path $StageDirectory -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "*$Pattern*" }
    if ($Found) { $Hits += $Found }
}

if ($Hits.Count -gt 0) {
    Write-Error "服务器 Stage 检测到不应存在的 VFX 客户端产物。"
    $Hits | ForEach-Object { Write-Host $_.FullName }
    exit 2
}

Write-Host "未通过文件名扫描发现 VFX 客户端产物。注意：最终仍需结合 AssetRegistry/IoStore 审计。"
exit 0
