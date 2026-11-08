Set-StrictMode -Version Latest

# 锁定正式竞技装配的代码及内容身份，由构建、Cook和运行参数复用。
$script:ArenaPlugins = @('DBAArena','DBAWorldPack_MainArena','GamePlatformArena','MobaPresentation')

function Get-FoundationWithoutArenaBuildArgument {
    '-DisablePlugin=' + ($script:ArenaPlugins -join '+')
}

function Get-FoundationWithoutArenaCookArguments {
    '-DisablePlugins=' + ($script:ArenaPlugins -join ',')
    '-ini:Game:[/Script/UnrealEd.ProjectPackagingSettings]:+DirectoriesToNeverCook=(Path=/DBAWorldPack_MainArena)'
    '-ini:Game:[/Script/Engine.AssetManagerSettings]:+DirectoriesToExclude=(Path=/DBAWorldPack_MainArena)'
}

function Assert-FoundationTargetAssembly {
    param([Parameter(Mandatory)]$Receipt, [Parameter(Mandatory)][ValidateSet('Client','Server')][string]$Target,
          [switch]$WithoutArena)
    if ($Receipt.TargetName -cne "DivineBeastsArena$Target" -or $Receipt.TargetType -cne $Target -or
        $Receipt.Platform -cne 'Win64' -or $Receipt.Configuration -cne 'Development' -or -not $Receipt.BuildPlugins.Count) {
        throw "$Target 目标收据身份或插件清单不符合 Win64 Development。"
    }
    $selectedArena = @($script:ArenaPlugins | Where-Object { $_ -in $Receipt.BuildPlugins })
    if ($WithoutArena) {
        if ($selectedArena.Count) { throw "无竞技收据仍含竞技插件：$($selectedArena -join ', ')" }
        $base = if ($Target -eq 'Server') { 'DBAServer' } else { 'DBAClient' }
        foreach ($required in @($base,'DBAWorldPack_Village')) {
            if ($required -notin $Receipt.BuildPlugins) { throw "无竞技收据缺少普通世界依赖：$required" }
        }
    } else {
        $missing = @($script:ArenaPlugins | Where-Object { $_ -notin $selectedArena -and ($Target -eq 'Client' -or $_ -ne 'MobaPresentation') })
        if ($missing.Count) { throw "默认装配缺少竞技插件：$($missing -join ', ')" }
    }
    [pscustomobject]@{ Assembly=if ($WithoutArena) { 'WithoutArena' } else { 'Default' }; ArenaPlugins=$selectedArena }
}

function Read-FoundationTargetAssembly {
    param([Parameter(Mandatory)][string]$Workspace, [Parameter(Mandatory)][ValidateSet('Client','Server')][string]$Target,
          [switch]$WithoutArena)
    $path = Join-Path $Workspace "Game/Binaries/Win64/DivineBeastsArena$Target.target"
    $receipt = Get-Content -Raw -LiteralPath $path | ConvertFrom-Json
    $proof = Assert-FoundationTargetAssembly -Receipt $receipt -Target $Target -WithoutArena:$WithoutArena
    $proof | Add-Member -NotePropertyName ReceiptPath -NotePropertyValue $path
    $proof | Add-Member -NotePropertyName ReceiptSha256 -NotePropertyValue (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    $proof
}

Export-ModuleMember -Function Get-FoundationWithoutArenaBuildArgument,Get-FoundationWithoutArenaCookArguments,Assert-FoundationTargetAssembly,Read-FoundationTargetAssembly
