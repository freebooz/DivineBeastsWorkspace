#requires -Version 7.0
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Import-Module (Join-Path $workspace 'Build/Game/FoundationTargetAssembly.psm1') -Force

$expectedArena = @('DBAArena','DBAWorldPack_MainArena','GamePlatformArena','MobaPresentation')
$compileOption = Get-FoundationWithoutArenaBuildArgument
if ($compileOption -cne ('-DisablePlugin=' + ($expectedArena -join '+'))) {
    throw '无竞技构建必须显式禁用四个竞技插件。'
}
$cookOptions = @(Get-FoundationWithoutArenaCookArguments)
if (($cookOptions -join ' ') -cnotmatch '-DisablePlugins=DBAArena,DBAWorldPack_MainArena,GamePlatformArena,MobaPresentation' -or
    ($cookOptions -join ' ') -cnotmatch '\+DirectoriesToExclude=\(Path=/DBAWorldPack_MainArena\)' -or
    ($cookOptions -join ' ') -cnotmatch '\+DirectoriesToNeverCook=\(Path=/DBAWorldPack_MainArena\)') {
    throw 'Cook与运行参数必须同时排除竞技插件、烘焙路径和资产扫描。'
}

$baseReceipt = @{ TargetName='DivineBeastsArenaServer'; TargetType='Server'; Platform='Win64'; Configuration='Development' }
$receipt = [pscustomobject]($baseReceipt + @{ BuildPlugins=@('DBAServer','DBAWorldPack_Village') })
$result = Assert-FoundationTargetAssembly -Receipt $receipt -Target Server -WithoutArena
if ($result.Assembly -ne 'WithoutArena') { throw '无竞技回执缺少独立装配身份。' }

foreach ($plugin in $expectedArena) {
    $wrong = [pscustomobject]($baseReceipt + @{ BuildPlugins=@('DBAServer','DBAWorldPack_Village',$plugin) })
    try { $null = Assert-FoundationTargetAssembly -Receipt $wrong -Target Server -WithoutArena; throw '接受了含竞技的旧回执。' }
    catch { if ($_.Exception.Message -notlike "*仍含竞技插件*$plugin*") { throw } }
}

try { $null = Assert-FoundationTargetAssembly -Receipt $receipt -Target Server; throw '接受了缺竞技的默认回执。' }
catch { if ($_.Exception.Message -notlike '*默认装配缺少*') { throw } }
Write-Output '无竞技编译参数、Cook扫描和回执正反例通过。'
