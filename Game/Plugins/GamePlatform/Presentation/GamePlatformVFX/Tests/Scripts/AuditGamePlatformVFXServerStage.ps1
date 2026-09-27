[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$StageDirectory)

$ErrorActionPreference = 'Stop'
$StageDirectory = [IO.Path]::GetFullPath($StageDirectory)
if (-not (Test-Path $StageDirectory -PathType Container)) { throw ('Stage directory not found: ' + $StageDirectory) }

$patterns = @(
    'GamePlatformVFXClient', 'GamePlatformVFXEditor', 'L_GPVFX_Review',
    'FXS_GPVFX_', 'FXE_GPVFX_', 'FXM_GPVFX_', 'FXT_GPVFX_',
    'DA_VFX_Platform_', 'DA_VFXCAT_Platform_'
)
$hits = [Collections.Generic.List[object]]::new()
foreach ($file in @(Get-ChildItem $StageDirectory -Recurse -File)) {
    foreach ($pattern in $patterns) {
        if ($file.FullName.IndexOf($pattern, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
            $hits.Add([pscustomobject]@{ pattern=$pattern; path=$file.FullName })
        }
    }
}
$result = [ordered]@{
    scope = 'server-stage-vfx-leak-audit'
    passed = ($hits.Count -eq 0)
    stage = $StageDirectory
    findings = @($hits)
    note = 'Packed container contents and AssetRegistry dependencies require separate UE-aware inspection.'
}
$result | ConvertTo-Json -Depth 5
if ($hits.Count -gt 0) { exit 1 }
exit 0
