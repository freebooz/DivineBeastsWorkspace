# SSLM 酉鸡V6 纹理包审计：只读，不启动UE，不伪造Texture2D。
[CmdletBinding()]
param([switch]$RequireZip)

$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$planPath=Join-Path $workspace 'Docs/Implementation/GamePlatformAbilitySystem/RoosterV6SkillIconImportManifest_20261019.json'
$plan=Get-Content -LiteralPath $planPath -Raw -Encoding utf8 | ConvertFrom-Json -Depth 64
$reg=Get-Content -LiteralPath (Join-Path $workspace 'Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json') -Raw -Encoding utf8 | ConvertFrom-Json -Depth 64
$source=@($reg.skills | Where-Object heroId -eq 'Hero.Zodiac.Rooster' | Sort-Object displayOrder)
if($plan.heroId -ne 'Hero.Zodiac.Rooster' -or $source.Count -ne 8 -or $plan.entries.Count -ne 8){throw '真实V6八技能基线缺失'}
for($i=0;$i -lt 8;$i++){
    $a=$plan.entries[$i];$b=$source[$i]
    if($a.order -ne ($i+1) -or $a.displayNameZh -ne $b.displayNameV6 -or $a.abilityId -ne $b.abilityId -or $a.slotId -ne $b.slotId){
        throw ('V6技能映射不一致：第'+($i+1)+'槽')
    }
}
$zipPath=Join-Path $workspace $plan.zipRelativePath
if(-not (Test-Path -LiteralPath $zipPath -PathType Leaf)){
    Write-Output 'ROOSTER_V6_MANIFEST_REGISTRY_OK=8'
    Write-Output ('ROOSTER_V6_ZIP_WAITING=' + $zipPath)
    if($RequireZip){throw '缺少ChatGPT导出的8技能纹理ZIP，请先放入Game/Saved/ArtReview/RoosterV6'}
    return
}
$ziphash=(Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
if($ziphash -ne $plan.zipSha256){throw '完整ZIP SHA256不匹配'}
Add-Type -AssemblyName System.IO.Compression
$z=[System.IO.Compression.ZipFile]::OpenRead($zipPath)
try{
    $unique=[Collections.Generic.HashSet[string]]::new()
    foreach($row in $plan.entries){
        foreach($spec in @(@($row.runtime48,48),@($row.review64,64),@($row.master,1254))){
            $relative=[string]$spec[0]
            $expected=[int]$spec[1]
            if($relative.StartsWith('/') -or $relative -match '(^|/)\.\.(/|$)'){throw '不安全ZIP路径'}
            $entry=$z.GetEntry($relative)
            if(-not $entry){throw ('缺少图片：'+$relative)}
            $stream=$entry.Open()
            $ms=[IO.MemoryStream]::new()
            try{$stream.CopyTo($ms);$bytes=$ms.ToArray()}
            finally{$stream.Dispose();$ms.Dispose()}
            if($bytes.Length -lt 24 -or $bytes[0] -ne 137 -or $bytes[1] -ne 80 -or $bytes[2] -ne 78 -or $bytes[3] -ne 71){throw ('非PNG图像：'+$relative)}
            $width=($bytes[16] -shl 24) -bor ($bytes[17] -shl 16) -bor ($bytes[18] -shl 8) -bor $bytes[19]
            $height=($bytes[20] -shl 24) -bor ($bytes[21] -shl 16) -bor ($bytes[22] -shl 8) -bor $bytes[23]
            if($width -ne $expected -or $height -ne $expected){throw ('分辨率错误：'+$relative)}
            if($expected -eq 48){
                $sha=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
                if($sha -ne $row.sha256_48){throw ('正式图标SHA不符：'+$relative)}
                if(-not $unique.Add($sha)){throw ('重复48纹理：'+$relative)}
            }
        }
    }
    Write-Output 'ROOSTER_V6_REGISTRY_AND_ZIP_PREFLIGHT_OK=8'
    Write-Output ('ROOSTER_V6_NATIVE_48_ASSET_READY_TO_IMPORT=' + $unique.Count)
}
finally{$z.Dispose()}
