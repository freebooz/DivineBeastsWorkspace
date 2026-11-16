#requires -Version 7.0
# 正式构建、Cook 与本地验证启动共用的制品证明。只接受 Foundation 独占目录。
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'FoundationBuildEvidence.psm1')

function Get-LocalValidationWorldMap {
    param([ValidateSet('Village','MainArena')][string]$Role)
    if ($Role -ceq 'Village') { '/DBAWorldPack_Village/Maps/L_Village_Start' }
    else { '/DBAWorldPack_MainArena/Maps/L_DBA_MainArena' }
}

function Get-ValidationRoleProfiles([string]$Workspace) {
    $profiles = @{}
    foreach ($role in 'Village','MainArena') {
        $path = Join-Path $Workspace "Deploy/Server/$role/server-profile.json"
        $bytes = [IO.File]::ReadAllBytes($path)
        $profile = [Text.Encoding]::UTF8.GetString($bytes).TrimStart([char]0xFEFF) | ConvertFrom-Json -AsHashtable
        $experience = if ($role -ceq 'Village') { 'Experience.Village.Tutorial' } else { 'Experience.MainArena.Main' }
        $policy = if ($role -ceq 'Village') { 'ExperienceInstance' } else { 'PerMatch' }
        if ($profile.serverRoleId -cne "GameServer.Role.$role" -or $profile.defaultExperienceId -cne $experience -or
            $profile.allowedExperienceIds -cnotcontains $experience -or $profile.instancePolicy -cne $policy -or
            $profile.worldPackage -cne (Get-LocalValidationWorldMap $role) -or -not $profile.requiredAssets.Count -or
            $profile.readiness.requireRequiredAssets -ne $true -or $profile.readiness.requireWorldBeginPlay -ne $true -or
            $profile.readiness.requireControlPlaneRegistration -ne $true) { throw "$role 正式角色配置或就绪合同不匹配。" }
        $profiles[$role] = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
    }
    return $profiles
}

function Get-LocalValidationStageHash {
    param([Parameter(Mandatory)][string]$StageDirectory, [ValidateSet('Client','Server')][string]$Target)
    if ((Get-Item -LiteralPath $StageDirectory -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Stage 不接受链接目录。' }
    $stage = Join-Path $StageDirectory "Windows$Target"
    $project = Join-Path $stage 'DivineBeastsArena'
    $pak = Join-Path $project 'Content/Paks'
    $exe = Join-Path $project "Binaries/Win64/DivineBeastsArena$Target.exe"
    $manifest = Join-Path $stage 'Manifest_UFSFiles_Win64.txt'
    $required = @($exe,$manifest) + @(foreach ($suffix in 'pak','utoc','ucas') { Join-Path $pak "DivineBeastsArena-Windows$Target.$suffix" })
    $files = @($required) + @(Get-ChildItem -LiteralPath $stage -Recurse -File -Force | Where-Object {
        $_.Extension -in '.exe','.dll','.modules','.ini','.pak','.utoc','.ucas' -and
        $_.FullName -notmatch '[\\/]Saved[\\/](?:Logs|Config|Crashes|SaveGames)[\\/]'
    } | Select-Object -ExpandProperty FullName)
    # Windows 实际加载使用制品路径。拒绝链接及读取中变化。
    $paths = @($files | Sort-Object -Unique -CaseSensitive)
    $records = foreach ($path in $paths) {
        $item = Get-Item -LiteralPath $path -Force
        if ($item.PSIsContainer -or -not $item.Length) { throw "Stage 运行文件缺失或为空：$path" }
        $cursor = $item
        while ($cursor.FullName -ine $StageDirectory) {
            if ($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Stage 不接受链接：$($cursor.FullName)" }
            $cursor = if ($cursor -is [IO.FileInfo]) { $cursor.Directory } else { $cursor.Parent }
        }
        $stamp = $item.LastWriteTimeUtc.Ticks
        $length = $item.Length
        $stream = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        try { $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)) }
        finally { $stream.Dispose() }
        $item.Refresh()
        if ($item.Length -ne $length -or $item.LastWriteTimeUtc.Ticks -ne $stamp) { throw "Stage 文件在检查中变化：$path" }
        [pscustomobject]@{ Path=[IO.Path]::GetRelativePath($stage,$path).Replace('\','/'); SHA256=$hash; IsExe=($path -ieq $exe) }
    }
    $text = ($records | ForEach-Object { "$($_.Path)`t$($_.SHA256)" }) -join "`n"
    [pscustomobject]@{
        Executable=$exe; ExecutableSHA256=($records | Where-Object IsExe).SHA256
        StageSHA256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($text)))
    }
}

function Assert-LocalValidationStageMaps {
    param([Parameter(Mandatory)][string]$ContainerCsv, [Parameter(Mandatory)][string]$StageDirectory,
        [ValidateSet('Client','Server')][string]$Target, [Parameter(Mandatory)][string[]]$Maps,
        [Parameter(Mandatory)][string]$CustomConfig)
    foreach ($map in $Maps) {
        $mount, $leaf = $map.TrimStart('/').Split('/', 2)
        if (@(Select-String -LiteralPath $ContainerCsv -SimpleMatch -Pattern "$mount/Content/$leaf.umap").Count -ne 1) {
            throw "Stage 缺少唯一地图 $map"
        }
    }
    $ufs = Join-Path $StageDirectory "Windows$Target/Manifest_UFSFiles_Win64.txt"
    foreach ($name in 'DefaultGame.ini','DefaultEngine.ini','DefaultGameplayTags.ini') {
        $relative = "DivineBeastsArena/Config/Custom/$CustomConfig/$name"
        if (@(Select-String -LiteralPath $ufs -SimpleMatch -Pattern $relative).Count -ne 1) { throw "Stage 缺少开发配置 $relative" }
    }
}

function Complete-LocalValidationCookProof {
    param([string]$Workspace,[string]$StageDirectory,[ValidateSet('Client','Server')][string]$Target,
        [string]$ContainerCsv,[string]$BattleInputSHA256)
    if ((Get-FoundationBattleInputSHA256 -Workspace $Workspace) -cne $BattleInputSHA256) {
        throw "$Target Cook/Stage 期间战斗合同变化。"
    }
    $maps = @('Village','MainArena' | ForEach-Object { Get-LocalValidationWorldMap $_ })
    Assert-LocalValidationStageMaps -ContainerCsv $ContainerCsv -StageDirectory $StageDirectory -Target $Target -Maps $maps -CustomConfig "VillageDevelopment$Target"
    $products = Get-LocalValidationStageHash -StageDirectory $StageDirectory -Target $Target
    [pscustomobject]@{
        BattleInputSHA256=$BattleInputSHA256; StageSHA256=$products.StageSHA256; ExecutableSHA256=$products.ExecutableSHA256
        Maps=$maps; RoleProfiles=(Get-ValidationRoleProfiles $Workspace)
    }
}

function Read-LocalValidationFoundationResult {
    param([string]$Workspace,[guid]$RunId,[ValidateSet('Build','Cook-FrontEndClient','Cook-Server')][string]$Operation)
    $directory = Join-Path $Workspace "Saved/Validation/FoundationM0/$RunId/$Operation"
    $path = Join-Path $directory 'result.json'
    try {
        $cursor = Get-Item -LiteralPath $path -Force
        while ($cursor.FullName -ine $Workspace) {
            if ($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw '结果路径包含链接' }
            $cursor = if ($cursor -is [IO.FileInfo]) { $cursor.Directory } else { $cursor.Parent }
        }
        $result = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json -AsHashtable
        if ($result.Operation -cne $Operation -or $result.RunId -cne $RunId.ToString('D') -or $result.Status -cne 'Passed' -or
            $null -eq $result.ExitCode -or $result.ExitCode -ne 0 -or $result.EngineVersion -cne '5.8.0' -or
            [IO.Path]::GetFullPath($result.Workspace) -ine $Workspace) { throw '不是同一工作区的成功 UE5.8 Foundation 操作' }
    } catch { throw "Foundation $Operation 证据未通过（$RunId）：$($_.Exception.Message)" }
    return $result
}

function Read-LocalValidationCookPackage {
    param([string]$Workspace,[guid]$RunId,[ValidateSet('Client','Server')][string]$Target)
    $operation = if ($Target -ceq 'Client') { 'Cook-FrontEndClient' } else { 'Cook-Server' }
    $result = Read-LocalValidationFoundationResult $Workspace $RunId $operation
    $stage = Join-Path $Workspace "Saved/Validation/FoundationM0/$RunId/$operation/Stage"
    $exe = Join-Path $stage "Windows$Target/DivineBeastsArena/Binaries/Win64/DivineBeastsArena$Target.exe"
    $proof = $result.LocalValidationPackage
    if ($null -eq $proof -or $proof.BattleInputSHA256 -cnotmatch '^[A-F0-9]{64}$' -or $proof.StageSHA256 -cnotmatch '^[A-F0-9]{64}$' -or
        $result.Target -cne $Target -or $result.StageDirectory -ine $stage -or $result.Executable -ine $exe -or
        $result.CustomConfig -cne "VillageDevelopment$Target" -or $result.TargetAssembly.Assembly -cne 'Default' -or
        $result.Result.ExitCode -ne 0 -or $result.Result.TimedOut -or -not $result.Result.ProcessId) {
        throw "Foundation $Target Cook/Stage 证据不完整（$RunId），请以村庄与主竞技场开发配置重新烘焙。"
    }
    $profiles = Get-ValidationRoleProfiles $Workspace
    foreach ($role in 'Village','MainArena') {
        if ($proof.RoleProfiles[$role] -cne $profiles[$role] -or $proof.Maps -cnotcontains (Get-LocalValidationWorldMap $role)) {
            throw "$Target Cook 缺少 $role 当前角色配置或地图证明。"
        }
    }
    $products = Get-LocalValidationStageHash -StageDirectory $stage -Target $Target
    if ($proof.StageSHA256 -cne $products.StageSHA256 -or $proof.ExecutableSHA256 -cne $products.ExecutableSHA256) {
        throw "$Target 已安装 Stage 制品与 Cook SHA256 不一致（$RunId）。"
    }
    return @{ Result=$result; Proof=$proof; Executable=$products.Executable; BuildVersion="1.0.0-validation-$($proof.StageSHA256)" }
}

Export-ModuleMember -Function Get-LocalValidationWorldMap,Get-LocalValidationStageHash,Complete-LocalValidationCookProof,Read-LocalValidationFoundationResult,Read-LocalValidationCookPackage
