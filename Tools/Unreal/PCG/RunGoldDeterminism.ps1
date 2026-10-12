#requires -Version 5.1
<#
.SYNOPSIS
PCG G09（同随机种子确定性）真实实例双进程验收。
.DESCRIPTION
仅在最新UE5.8 PCGEditor已正式编译、无并行Editor/UBT时调用两次GeneratePreview。
每次都生成/清理临时实例、不保存地图；以实际ISM网格/量化变换SHA-1指纹比较，
只有三种网格全部相同才判定G09局部通过；不代替G01-G16全项或双端Cook。
#>
[CmdletBinding()]
param([string]$EngineRoot = $env:UE_ROOT)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$probe = Join-Path $PSScriptRoot 'RunGoldSpatialProbe.ps1'
$evidence = Join-Path $root ('Saved/Validation/GamePlatformPCG/GoldDeterminism/' + [guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$required = @('PCG_Canopy','PCG_Rock','PCG_Crops')
$runs = [Collections.Generic.List[object]]::new()

try {
    if (!$EngineRoot -or !(Test-Path $probe)) { throw '缺少锁定UE5.8目录或受控预览入口。' }
    for ($index = 1; $index -le 2; $index++) {
        # 实际生成任务必须由同一个安全入口来保护；不绕过其Editor/UBT占用、DLL新鲜度、资源哈希门禁。
        $messages = @(& $probe -EngineRoot $EngineRoot -Stage GeneratePreview 2>&1)
        $receipt = @($messages | Where-Object {
            "$_" -match 'GOLD_SPAWNER_GENERATION_PREVIEW_VALIDATED_IN_UE'
        })
        $locations = @($messages | ForEach-Object {
            $match = [regex]::Match("$_", '^GoldLevel GeneratePreview证据目录：(.+)$')
            if ($match.Success) { $match.Groups[1].Value }
        })
        if ($receipt.Count -ne 1 -or $locations.Count -ne 1) {
            throw "第$index次真实PCG预览失败：$($messages -join [Environment]::NewLine)"
        }
        $log = Join-Path $locations[0] 'UE_Editor_GeneratePreview.stdout.log'
        if (!(Test-Path -LiteralPath $log)) { throw "第$index次缺少UE实际生成日志：$log" }

        $seen = @{}
        $pattern = 'GP_PCG_PREVIEW_RESULT Label=(PCG_(?:Canopy|Rock|Crops)) Instances=([0-9]+) Fingerprint=(sha1-ism-mm-v1:[A-Fa-f0-9]{40})'
        foreach ($line in Get-Content -LiteralPath $log -Encoding UTF8) {
            $match = [regex]::Match($line, $pattern)
            if (!$match.Success) { continue }
            $label = $match.Groups[1].Value
            $count = [long]::Parse($match.Groups[2].Value)
            if ($seen.ContainsKey($label) -or $count -le 0 -or $count -gt 20000) {
                throw "第$index次出现重复/无效的实际实例统计：$label"
            }
            $seen[$label] = @{
                Instances = $count
                Fingerprint = $match.Groups[3].Value.ToLowerInvariant()
            }
        }
        foreach ($label in $required) {
            if (!$seen.ContainsKey($label)) {
                throw "第$index次缺少批准图的真实Spawner实例+指纹：$label"
            }
        }
        $runs.Add(@{ Number = $index; UELog = $log; Results = $seen })
        Write-Output ("G09_UE_PREVIEW_RUN_"+$index+"_PASS")
    }

    foreach ($label in $required) {
        $first = $runs[0].Results[$label]
        $second = $runs[1].Results[$label]
        if ($first.Instances -ne $second.Instances -or
            $first.Fingerprint -ne $second.Fingerprint) {
            throw ("G09同种子两个独立UE进程结果不一致：$label，" +
                "第一次=$($first.Instances)/$($first.Fingerprint)，第二次=$($second.Instances)/$($second.Fingerprint)")
        }
    }
    $report = @{
        Scope = '同UE5.8版本两独立编辑器进程、3份已批准Gold装饰图，不含G01-G16其它项'
        Passed = $true
        Runs = $runs.ToArray()
    } | ConvertTo-Json -Depth 9
    $report | Set-Content -LiteralPath (Join-Path $evidence 'G09_report.json') -Encoding UTF8
    Write-Output 'GOLD_G09_TWO_FRESH_EDITORS_DETERMINISM_PASS'
    Write-Output '只证明本次指定图和同种子可复现；不等于G01-G16全项、NavMesh或Server/Client Cook验收。'
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
finally {
    Write-Output ('G09证据目录：'+$evidence)
}
