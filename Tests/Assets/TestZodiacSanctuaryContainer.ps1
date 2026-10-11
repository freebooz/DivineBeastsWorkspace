#requires -Version 7.0
<#
.SYNOPSIS
验证真实客户端IoStore是否完整携带神兽圣境网格、PBR材质/贴图和工作室。
.DESCRIPTION
从Blender制作清单读取本轮真实资产身份，对UE容器CSV逐项检查非空ExportBundleData。
只读输入，不启动服务、不修改源资产；报告是发布闭包证据，不是人工视觉验收。
#>
param([Parameter(Mandatory)][string]$ContainerCsv,[Parameter(Mandatory)][string]$ReportPath)
$ErrorActionPreference='Stop'
try {
    $workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $manifest=Get-Content -Raw -Encoding utf8 -LiteralPath (Join-Path $workspace 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAFrontEndPack/SourceArt/Blender/ZodiacSanctuary/SceneManifest.json') | ConvertFrom-Json
    $prefix='DBAFrontEndPack/Content/CharacterPreview/Environment/ZodiacSanctuary/'
    $required=@('DBAFrontEndPack/Content/Maps/L_DBA_CharacterStudio.umap')
    $required+=@($manifest.objects | ForEach-Object { $prefix+'Meshes/'+$_.name+'.uasset' })
    $required+=@($manifest.materials | ForEach-Object { $prefix+'Materials/'+$_.name+'.uasset' })
    foreach ($record in $manifest.materials) {
        foreach ($key in @('texture','normal_texture','orm_texture')) {
            if ($record.$key) { $required+=$prefix+'Textures/'+[IO.Path]::GetFileNameWithoutExtension($record.$key)+'.uasset' }
        }
    }
    $required=@($required | Sort-Object -Unique)
    $rows=@(Import-Csv -LiteralPath $ContainerCsv | ForEach-Object {
        $entry=@{};foreach ($property in $_.PSObject.Properties) { $entry[$property.Name.Trim()]=([string]$property.Value).Trim() };$entry
    })
    if (!$rows.Count -or !$rows[0].ContainsKey('Filename') -or !$rows[0].ContainsKey('Size')) { throw '不是有效UE容器CSV。' }
    $missing=@($required | Where-Object {
        $suffix=$_
        -not @($rows | Where-Object { $filename=$_.Filename.Replace('\','/'); ($filename -eq $suffix -or $filename.EndsWith('/'+$suffix,[StringComparison]::Ordinal)) -and $_.ChunkType -eq 'ExportBundleData' -and [long]$_.Size -gt 0 }).Count
    })
    $result=@{Passed=($missing.Count -eq 0);RequiredCount=$required.Count;Missing=$missing;ContainerCsv=[IO.Path]::GetFullPath($ContainerCsv)}
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ReportPath -Encoding utf8
    $result | ConvertTo-Json -Depth 4
    if ($missing.Count) { exit 1 }
    exit 0
} catch { Write-Error $_;exit 1 }
