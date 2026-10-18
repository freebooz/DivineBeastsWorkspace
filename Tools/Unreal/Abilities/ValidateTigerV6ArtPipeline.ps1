# 寅虎V6技能图标九阶段门禁：冻结设计→24张候选→人工3选1→8套64/48成品。
# 项目第三层独立审查脚本；只读文件系统与JSON，不生成图片、不修改.uasset和GAS授权。
# 只验证合同：pwsh -NoProfile -File Tools/Unreal/Abilities/ValidateTigerV6ArtPipeline.ps1
# 检查24份候选真实存在：追加 -VerifyCandidateFiles
# 检查八份人工签收：再追加 -VerifySelections
# 检查母版/64与48纹理：再追加 -VerifyFinalImages
[CmdletBinding()]
param(
    [switch]$VerifyCandidateFiles,
    [switch]$VerifySelections,
    [switch]$VerifyFinalImages
)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$dir=Join-Path $repo 'Docs/Implementation/GamePlatformAbilitySystem'
$freeze=Get-Content -LiteralPath (Join-Path $dir 'TigerV6EightSkillsArtLock_20261018.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$draft=Get-Content -LiteralPath (Join-Path $dir 'TigerV6IconCandidates_24_20261018.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$selections=Get-Content -LiteralPath (Join-Path $dir 'TigerV6IconSelections_20261018.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$final=Get-Content -LiteralPath (Join-Path $dir 'TigerV6SkillIconImportManifest_Draft.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$registry=Get-Content -LiteralPath (Join-Path $dir 'V6SkillImplementationRegistry_20261018.json') -Raw -Encoding UTF8 | ConvertFrom-Json

# 统一中文失败信息；不能在缺图时把只读静态门禁冒充素材验收。
function Assert-Truth([bool]$Condition,[string]$Message) {
    if (-not $Condition) {throw $Message}
}

# 严格核实文件签名、PNG IHDR、尺寸和8位真彩，而不仅根据扩展名或人工填报的尺寸判断。
function Read-IconPng([string]$Relative) {
    $path=Join-Path $repo $Relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {throw "找不到要求的PNG：$Relative"}
    [byte[]]$data=[IO.File]::ReadAllBytes($path)
    [byte[]]$sig=@(137,80,78,71,13,10,26,10)
    if ($data.Length -lt 33) {throw "PNG尺寸不足33字节：$Relative"}
    for($k=0;$k -lt 8;$k++) {
        if($data[$k] -ne $sig[$k]) {throw "PNG签名错误：$Relative"}
    }
    if([Text.Encoding]::ASCII.GetString($data,12,4) -ne 'IHDR') {throw "PNG首块不是IHDR：$Relative"}
    $w=[int](($data[16] -shl 24) -bor ($data[17] -shl 16) -bor ($data[18] -shl 8) -bor $data[19])
    $h=[int](($data[20] -shl 24) -bor ($data[21] -shl 16) -bor ($data[22] -shl 8) -bor $data[23])
    if($data[24] -ne 8 -or $data[25] -notin @(2,6)) {throw "PNG必须使用8位RGB或RGBA真彩：$Relative"}
    $sha=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    return @{Width=$w;Height=$h;Sha256=$sha}
}

# 限定本次锁定为纯美术输入；不会绕过生产技能的服务器审批或原有V5显示名。
Assert-Truth ($freeze.heroId -eq 'Hero.Zodiac.Tiger') '冻结定义的英雄身份不正确'
Assert-Truth ($freeze.designState -eq 'ART_DIRECTION_FROZEN_NOT_SERVER_APPROVED') '冻结状态不可冒充服务器正式批准'
Assert-Truth ($freeze.designLockId -eq $draft.designLock -and $draft.designLock -eq $selections.designLock) '三份设计/候选/选择清单冻结ID不一致'
Assert-Truth (@($freeze.skills).Count -eq 8) '冻结表必须有8技能'
Assert-Truth (@($final.entries).Count -eq 8) '最终导入清单必须有8技能'
Assert-Truth (@($draft.candidates).Count -eq 24 -and $draft.candidateCountRequired -eq 24) '必须恰好有24条候选登记'
Assert-Truth (@($selections.entries).Count -eq 8) '人工优选清单必须有8条技能'
$real=@($registry.skills | Where-Object {$_.heroId -eq 'Hero.Zodiac.Tiger'})
Assert-Truth ($real.Count -eq 8) '权威V6注册表寅虎技能数量不是8'
$requiredSlots=@('Primary','Slot1','Slot2','Slot4','Slot3','Slot5','Slot6','Slot7')
$knownCandidateIds=@{}
$knownFiles=@{}
$knownPrompts=@{}
for($i=0;$i -lt 8;$i++) {
    $f=$freeze.skills[$i]
    $out=$final.entries[$i]
    $src=@($real | Where-Object {$_.displayOrder -eq ($i+1)})
    Assert-Truth ($src.Count -eq 1) "第$($i+1)技能的V6源注册项不是唯一"
    $s=$src[0]
    Assert-Truth ($f.order -eq ($i+1) -and $f.inputSlot -eq $requiredSlots[$i]) "冻结定义第$($i+1)项排序/输入槽错误"
    Assert-Truth ($s.slotId -eq $f.inputSlot -and $s.abilityId -eq $f.abilityId -and $s.displayNameV6 -eq $f.v6NameZh -and $s.approvedNameV5 -eq $f.v5ApprovedDisplayNameZh -and $s.mechanismV6 -eq $f.v6EffectZh) "冻结设计第$($i+1)项与历史V6权威来源不一致"
    Assert-Truth ($out.order -eq ($i+1) -and $out.inputSlot -eq $f.inputSlot -and $out.abilityId -eq $f.abilityId -and $out.displayNameV6ProposalZh -eq $f.v6NameZh) "第$($i+1)项正式目标清单与冻结设计不一致"
    $ballot=$selections.entries[$i]
    Assert-Truth ($ballot.skillOrder -eq ($i+1) -and $ballot.v6NameZh -eq $f.v6NameZh) "人工选票第$($i+1)项身份错误"
    $three=@($draft.candidates | Where-Object {$_.skillOrder -eq ($i+1)})
    Assert-Truth ($three.Count -eq 3) "第$($i+1)技能候选不等于三张"
    foreach($letter in @('A','B','C')) {
        $expectedId=('S{0:D2}-{1}' -f ($i+1),$letter)
        $matches=@($three | Where-Object {$_.candidateId -eq $expectedId -and $_.scheme -eq $letter})
        Assert-Truth ($matches.Count -eq 1) "候选编号/方案不唯一：$expectedId"
        $c=$matches[0]
        Assert-Truth ($c.abilityId -eq $f.abilityId -and $c.v6NameZh -eq $f.v6NameZh) "候选$expectedId所属技能不正确"
        Assert-Truth ($c.generationPromptZh.Length -gt 170 -and $c.generationPromptZh.Contains($f.v6NameZh)) "候选$expectedId提示词不完整"
        Assert-Truth ($c.pngFile -match ('/Candidates/S{0:D2}/' -f ($i+1)) -and $c.pngFile -match ('Candidate_{0}_1024.png$' -f $letter)) "候选$expectedId文件命名/位置错误"
        if($knownCandidateIds.ContainsKey($c.candidateId) -or $knownFiles.ContainsKey($c.pngFile) -or $knownPrompts.ContainsKey($c.generationPromptZh)) {throw "候选ID/源文件/提示词重复：$expectedId"}
        $knownCandidateIds[$c.candidateId]=$true
        $knownFiles[$c.pngFile]=$true
        $knownPrompts[$c.generationPromptZh]=$true
    }
    $expectedOptions=@('A','B','C') | ForEach-Object { 'S{0:D2}-{1}' -f ($i+1),$_ }
    Assert-Truth (($ballot.options -join ',') -eq ($expectedOptions -join ',')) "第$($i+1)项人审选票不是A/B/C三选"
}
Write-Output '门禁1通过：8技能V6美术冻结、稳定AbilityId与真实槽位一致；24份A/B/C候选提示词完全独立且无重名；生产技能仍未宣称被授予。'

# 素材路径与文件的真实性校验是独立阶段；没有图片则明确失败，不返回软通过。
if($VerifyCandidateFiles -or $VerifySelections -or $VerifyFinalImages) {
    $hashes=@{}
    foreach($c in $draft.candidates) {
        $png=Read-IconPng $c.pngFile
        Assert-Truth ($png.Width -eq $png.Height -and $png.Width -ge 1024) "候选$($c.candidateId)不是≥1024方图"
        Assert-Truth (-not $hashes.ContainsKey($png.Sha256)) "候选图哈希重复，疑似拼图复用或完全相同图：$($c.candidateId)"
        if($c.sourceSha256) {Assert-Truth ($c.sourceSha256.ToLowerInvariant() -eq $png.Sha256) "候选$($c.candidateId)登记哈希不符"}
        $hashes[$png.Sha256]=$true
    }
    Write-Output '门禁2通过：24份真实独立PNG尺寸达标、SHA256互不重复。艺术水平仍须人审。'
}
if($VerifySelections -or $VerifyFinalImages) {
    foreach($entry in $selections.entries) {
        $id=[string]$entry.selectedCandidateId
        Assert-Truth ($id.Length -gt 0 -and $id -in @($entry.options)) "第$($entry.skillOrder)项没有选中A/B/C候选"
        Assert-Truth ($entry.selectionStatus -eq 'APPROVED') "第$($entry.skillOrder)项未获明确人工批准"
        Assert-Truth (-not [string]::IsNullOrWhiteSpace($entry.reviewedBy)) "第$($entry.skillOrder)项缺真实审核人"
        $moment=[datetimeoffset]::MinValue
        Assert-Truth ([datetimeoffset]::TryParse([string]$entry.reviewedAt,[ref]$moment)) "第$($entry.skillOrder)项缺合法审核时间"
        Assert-Truth ($knownCandidateIds.ContainsKey($id)) "人工选择$($id)在候选登记中不存在"
    }
    Write-Output '门禁3通过：8项人工选优的记录字段完整；本门禁无法独立鉴别人工签名真实性。'
}
if($VerifyFinalImages) {
    $h64=@{}
    $h48=@{}
    for($i=0;$i -lt 8;$i++) {
        $out=$final.entries[$i]
        $master=Read-IconPng $out.masterFile
        Assert-Truth ($master.Width -eq $master.Height -and $master.Width -ge 1024) "第$($i+1)项高清终稿未达标"
        $name=[string]$out.assetName
        $p64=$out.runtimePngFile -replace '/48/','/64/'
        $png64=Read-IconPng $p64
        $png48=Read-IconPng $out.runtimePngFile
        Assert-Truth ($png64.Width -eq 64 -and $png64.Height -eq 64) "第$($i+1)项64像素不合格"
        Assert-Truth ($png48.Width -eq 48 -and $png48.Height -eq 48) "第$($i+1)项48像素不合格"
        Assert-Truth (-not $h64.ContainsKey($png64.Sha256) -and -not $h48.ContainsKey($png48.Sha256)) "不同技能的64或48纹理出现相同哈希"
        $h64[$png64.Sha256]=$true
        $h48[$png48.Sha256]=$true
        if($out.png48Sha256) {Assert-Truth ($out.png48Sha256.ToLowerInvariant() -eq $png48.Sha256) "第$($i+1)项48文件哈希不符合登记"}
        if($out.masterSha256) {Assert-Truth ($out.masterSha256.ToLowerInvariant() -eq $master.Sha256) "第$($i+1)项高清母版哈希不符合登记"}
    }
    Write-Output '门禁4通过：8套母版/64×64/48×48真实PNG文件静态验证。仍需UE5.8、HDR/颜色与HUD视觉人工验收。'
}
