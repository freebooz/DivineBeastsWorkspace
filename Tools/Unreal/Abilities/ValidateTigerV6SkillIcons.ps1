# 寅虎 V6 八技能图标生产清单和48像素文件门禁。
# 所属层级：DivineBeasts项目层；仅校验磁盘源图与契约，不创建/修改原生UE资产、游戏授权或服务器资源。
# 执行：pwsh -File Tools/Unreal/Abilities/ValidateTigerV6SkillIcons.ps1
# 全量出图后执行：pwsh -File Tools/Unreal/Abilities/ValidateTigerV6SkillIcons.ps1 -VerifyImages
[CmdletBinding()]
param(
    # 只有出图完成后才启用；没有素材时不得假报通过。
    [switch]$VerifyImages
)
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$manifestPath = Join-Path $root 'Docs/Implementation/GamePlatformAbilitySystem/TigerV6SkillIconImportManifest_Draft.json'
$registryPath = Join-Path $root 'Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json'

# 验证一个PNG文件的签名、像素尺寸与可用于sRGB图标的8位真彩格式。
# 注意：本函数不代替UE颜色空间/RHI像素回读、人工美术签审或引擎原生纹理检验。
function Read-PngHeader {
    param([string]$FilePath)
    if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) {
        throw "缺少PNG源文件：$FilePath"
    }
    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($FilePath)
    [byte[]]$sig = @(137, 80, 78, 71, 13, 10, 26, 10)
    if ($bytes.Length -lt 33) { throw "PNG字节不足：$FilePath" }
    for ($i=0; $i -lt 8; $i++) {
        if ($bytes[$i] -ne $sig[$i]) { throw "PNG文件签名错误：$FilePath" }
    }
    if ([System.Text.Encoding]::ASCII.GetString($bytes, 12, 4) -ne 'IHDR') {
        throw "PNG首块不是IHDR：$FilePath"
    }
    # 先转为32位整数再移位，避免PowerShell在8位字节类型上截断，误将1024识别为0。
    $width = (([int]$bytes[16] -shl 24) -bor ([int]$bytes[17] -shl 16) -bor ([int]$bytes[18] -shl 8) -bor [int]$bytes[19])
    $height = (([int]$bytes[20] -shl 24) -bor ([int]$bytes[21] -shl 16) -bor ([int]$bytes[22] -shl 8) -bor [int]$bytes[23])
    if ($bytes[24] -ne 8 -or $bytes[25] -notin @(2,6)) {
        throw "图标必须是8位RGB/RGBA真彩PNG：$FilePath"
    }
    return @{ Width=$width; Height=$height; ColorType=[int]$bytes[25] }
}

try {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $registry = Get-Content -LiteralPath $registryPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($manifest.heroId -ne 'Hero.Zodiac.Tiger') { throw '清单英雄身份错误' }
    if (@($manifest.entries).Count -ne 8) { throw '寅虎必须有且仅有八项技能' }
    $expected = @($registry.skills | Where-Object { $_.heroId -eq 'Hero.Zodiac.Tiger' })
    if ($expected.Count -ne 8) { throw 'V6源注册表寅虎技能数量不是8' }
    $slots = @('Primary','Slot1','Slot2','Slot4','Slot3','Slot5','Slot6','Slot7')
    $idSeen = @{}
    $nameSeen = @{}
    $hashSeen = @{}
    $checked = 0
    foreach ($entry in $manifest.entries) {
        $order = [int]$entry.order
        if ($order -ne ($checked + 1)) { throw "屏幕序号非连续：$order" }
        if ($entry.inputSlot -ne $slots[$checked]) { throw "槽位与实际显示顺序不一致：$order" }
        if ($entry.assetName -notmatch ('^T_DBA_Tiger_S' + ('{0:D2}' -f $order) + '_[A-Za-z0-9]+_V6$')) {
            throw "图标资产命名不合规：$($entry.assetName)"
        }
        if ($idSeen.ContainsKey([string]$entry.abilityId) -or $nameSeen.ContainsKey([string]$entry.assetName)) {
            throw '稳定AbilityId或资产名重复'
        }
        $idSeen[[string]$entry.abilityId] = $true
        $nameSeen[[string]$entry.assetName] = $true
        $source = @($expected | Where-Object { $_.displayOrder -eq $order })
        if ($source.Count -ne 1) { throw "V6源注册表槽位不唯一：$order" }
        $s = $source[0]
        if ($s.slotId -ne $entry.inputSlot -or $s.abilityId -ne $entry.abilityId -or
            $s.displayNameV6 -ne $entry.displayNameV6ProposalZh -or
            $s.approvedNameV5 -ne $entry.approvedDisplayNameV5Zh -or
            $s.mechanismV6 -ne $entry.v6MechanismZh) {
            throw "第$order项与V6注册表身份、名称或原始机制不符"
        }
        $shouldChange = ($entry.displayNameV6ProposalZh -ne $entry.approvedDisplayNameV5Zh)
        if ([bool]$entry.nameChangeRequiresReview -ne $shouldChange) {
            throw "第$order项更名审批状态不正确"
        }
        $expectedObject = '/DBAHeroPack_Tiger/UI/Abilities/{0}.{0}' -f $entry.assetName
        if ($entry.softObjectPath -ne $expectedObject) { throw "UE软对象路径错误：$order" }
        if ($VerifyImages) {
            $master = Join-Path $root $entry.masterFile
            $png48 = Join-Path $root $entry.runtimePngFile
            $m = Read-PngHeader -FilePath $master
            $p = Read-PngHeader -FilePath $png48
            if ($m.Width -ne $m.Height -or $m.Width -lt 1024) {
                throw "第$order项原稿必须为不小于1024的方图"
            }
            if ($p.Width -ne 48 -or $p.Height -ne 48) {
                throw "第$order项必须为真实48×48图像"
            }
            $h = (Get-FileHash -LiteralPath $png48 -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($hashSeen.ContainsKey($h)) { throw "两个技能使用同一份48像素PNG：$order" }
            $hashSeen[$h] = $true
            if ($entry.png48Sha256 -and $entry.png48Sha256.ToLowerInvariant() -ne $h) {
                throw "第$order项48像素PNG与已登记SHA256不一致"
            }
            Write-Output ("第{0}项48×48 PNG校验通过，SHA256={1}" -f $order,$h)
        }
        $checked++
    }
    if ($VerifyImages) {
        Write-Output '磁盘图像尺寸/身份/哈希静态验证通过。仍必须执行美术人工签审和UE5.8 Monolith导入、原生回读、HUD/Cook验收。'
    } else {
        Write-Output '寅虎V6八技能清单和原始机制与实施注册表完全对应；尚未执行素材检查，不代表PNG、UE纹理或GAS授权完成。'
    }
} catch {
    Write-Error ("寅虎V6图标门禁失败：" + $_.Exception.Message)
    exit 1
}
