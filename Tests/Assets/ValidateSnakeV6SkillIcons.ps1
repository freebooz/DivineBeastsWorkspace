# SSLM 巳蛇V6技能图标离线检查。
# 所属范围：第三层DBAHeroPack_Snake的原始美术文件；该脚本只读取PNG和JSON，不修改生产UE资产。
# 目的：验证八个技能的稳定身份、输入顺序、文件尺寸、PNG魔数、独立性、命名及SHA-256是否一致。
# 入参：项目工作空间根（默认根据此脚本所在位置向上推算）；输出：通过统计或失败详情。
# 注意：此检查不会加载UE编辑器、Monolith、GAS、UI Profile，也不能代替电影级美术人工审核、原生导入或客户端显示验收。
param([string]$WorkspaceRoot = '')
$ErrorActionPreference = 'Stop'

# 固定相对关系：Tests/Assets的祖父目录为工作空间根，避免绑定开发者电脑盘符。
if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
}
$manifestPath = Join-Path $WorkspaceRoot 'Docs/Implementation/GamePlatformAbilitySystem/SnakeV6SkillIconImportManifest_Draft.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "缺少蛇技能图标映射清单：$manifestPath"
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($manifest.heroId -ne 'Hero.Zodiac.Snake' -or $manifest.pack -ne 'DBAHeroPack_Snake') {
    throw '清单英雄身份或资源所有者不正确。'
}
$expectedSlots = @('Primary','Slot1','Slot2','Slot4','Slot3','Slot5','Slot6','Slot7')
$expectedKeys = @('scale_bite','venom_spear','serpent_arc','serpent_maelstrom',
                  'sinuous_dash','shed_skin','coil_silence','venom_mark')
$expectedNames = @('幽鳞刺','毒牙','双鳞弹','毒阵爆','蛇行','蜕鳞','蛇纹缚','毒径')
$expectedV6Names = @('幽鳞刺','毒牙','双鳞弹','万毒归鳞','蛇行','蜕鳞','缠纹','封途')
if (@($manifest.entries).Count -ne 8) { throw "技能条目数量错误：$(@($manifest.entries).Count)，应为8。" }
if ((@($manifest.inputSlotsOrder) -join ',') -ne ($expectedSlots -join ',')) {
    throw '技能栏显示次序错误，必须把Slot4终极放在第4显示格、Slot3移动放在第5格。'
}
if (@($manifest.masterPixels) -join 'x' -ne '1024x1024' -or
    @($manifest.runtimePixels) -join 'x' -ne '48x48') {
    throw '清单中母稿尺寸与最终运行图标尺寸不符合要求。'
}

Add-Type -AssemblyName System.Drawing -ErrorAction Stop
$uniqueIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$uniqueAssetNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$uniqueRuntimeHashes = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$uniqueMasterHashes = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)

for ($i=0; $i -lt 8; $i++) {
    $entry = $manifest.entries[$i]
    $index = $i+1
    $expectedId = "dba.ability.snake_eight_$($expectedKeys[$i])@1"
    if ($entry.order -ne $index -or $entry.slotId -ne $expectedSlots[$i] -or
        $entry.abilityId -cne $expectedId) {
        throw "第$index项稳定技能编号或槽位错误。"
    }
    if ($entry.displayNameCurrentV5Zh -cne $expectedNames[$i] -or
        $entry.displayNameProposedV6Zh -cne $expectedV6Names[$i]) {
        throw "第$index项已批准V5显示名或待审V6名称不匹配。"
    }
    if ($entry.v6NameRequiresApproval -ne ($expectedNames[$i] -cne $expectedV6Names[$i])) {
        throw "第$index项名称签审状态错误。"
    }
    if (-not $uniqueIds.Add([string]$entry.abilityId) -or
        -not $uniqueAssetNames.Add([string]$entry.textureName)) {
        throw "第$index项的技能ID或纹理文件名重复。"
    }
    if ($entry.textureName -notmatch "^T_DBA_Snake_S0$($index)_[A-Za-z][A-Za-z0-9]*_V6$") {
        throw "第$index项纹理命名不规范。"
    }
    $expectedUEPath = "/DBAHeroPack_Snake/UI/Abilities/$($entry.textureName)"
    if ($entry.expectedUEPackagePath -cne $expectedUEPath -or
        $entry.expectedUESoftObjectPath -cne "$expectedUEPath.$($entry.textureName)") {
        throw "第$index项UE纹理挂载路径错误。"
    }
    $pairs = @(
       @{ name='1024源稿'; relative=$entry.masterPng; width=1024; height=1024; expected=$entry.sha256Master; ext="_Master_1024.png" },
       @{ name='48像素'; relative=$entry.runtimePng; width=48; height=48; expected=$entry.sha256Runtime48; ext='.png' }
    )
    foreach($pair in $pairs) {
        # GetFullPath不能凭空证明路径受控，先检查相对路径不能逃逸当前工作空间。
        $relative = ([string]$pair.relative).Replace('/','\')
        $file = [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot $relative))
        $workspacePrefix = [System.IO.Path]::GetFullPath($WorkspaceRoot).TrimEnd('\') + '\'
        if (-not $file.StartsWith($workspacePrefix,[System.StringComparison]::OrdinalIgnoreCase)) {
            throw "第$index项文件路径超出工作空间：$file"
        }
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            throw "第$index项缺少$($pair.name)文件：$file"
        }
        $bytes = [System.IO.File]::ReadAllBytes($file)
        if ($bytes.Length -lt 100 -or (@($bytes[0..7] | ForEach-Object ToString 'X2') -join '') -ne '89504E470D0A1A0A') {
            throw "第$index项$($pair.name)不是有效PNG签名。"
        }
        $stream = [System.IO.MemoryStream]::new($bytes,$false)
        try {
            $picture = [System.Drawing.Image]::FromStream($stream,$true,$true)
            try {
                if ($picture.Width -ne $pair.width -or $picture.Height -ne $pair.height) {
                    throw "第$index项$($pair.name)的真实尺寸错误，应为$($pair.width)×$($pair.height)。"
                }
            }
            finally { $picture.Dispose() }
        } finally { $stream.Dispose() }
        $sha = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($sha -cne $pair.expected) { throw "第$index项$($pair.name)SHA-256不一致；素材修改后必须重新审核并更新清单。" }
        if ($pair.width -eq 48 -and -not $uniqueRuntimeHashes.Add($sha)) {
            throw "第$index项48像素PNG与其他技能重复，拒绝将同一图像充当八枚独立纹理。"
        }
        if ($pair.width -eq 1024 -and -not $uniqueMasterHashes.Add($sha)) {
            throw "第$index项1024像素源稿内容重复。"
        }
    }
    if ($entry.texture2DImportedVerified -or $entry.productionUIProfileMappedVerified -or
        $entry.UEColorValidated -or $entry.humanArtApproved) {
        throw "第$index项清单声称完成尚未实际验收的原生/美术步骤，检查人工证据后另修订状态。"
    }
    Write-Output ("校验通过 {0}/8：{1} → {2}，1024母稿与48像素PNG存在且SHA一致。" -f $index,$entry.displayNameProposedV6Zh,$entry.textureName)
}
if ($manifest.engineArtifactImported -ne 0 -or $manifest.uiProfileProductionModified -or
    -not $manifest.artGate.humanReviewPending) {
    throw '本草案的完成状态与未导入、未签审事实不一致。'
}
$folder = Join-Path $WorkspaceRoot 'Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Snake/SourceArt/UI/SkillIcons/V6'
if (@(Get-ChildItem (Join-Path $folder 'Masters') -Filter 'T_DBA_Snake_S*_V6_Master_1024.png').Count -ne 8 -or
    @(Get-ChildItem (Join-Path $folder '48') -Filter 'T_DBA_Snake_S*_V6.png').Count -ne 8) {
    throw '实际源美术图或运行48像素图文件数量不是各8份。'
}
Write-Output '最终通过：八技能身份与槽位一致，8份1024源图+8份48PNG尺寸/签名/散列/独立性检查全部通过。'
Write-Output '注意：这不代表3A美术人工验收、UE5.8导入、Profile绑定、GAS授权、Client显示或Server Cook已通过。'
