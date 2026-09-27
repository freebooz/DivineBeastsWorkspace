Set-StrictMode -Version Latest

function Get-InheritanceLayerInfo {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$PluginRoot
    )
    $full = [IO.Path]::GetFullPath($Path)
    $root = [IO.Path]::GetFullPath($PluginRoot).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    if (-not $full.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { return $null }
    $relative = $full.Substring($root.Length).TrimStart([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) -replace '\\','/'
    if ($relative.StartsWith('GamePlatform/')) { return [pscustomobject]@{ Name='GamePlatform'; Index=0 } }
    if ($relative.StartsWith('MobaCommon/')) { return [pscustomobject]@{ Name='MobaCommon'; Index=1 } }
    if ($relative.StartsWith('DivineBeasts/')) { return [pscustomobject]@{ Name='DivineBeasts'; Index=2 } }
    return $null
}

function Remove-CppComments {
    param([Parameter(Mandatory)][string]$Text)
    $withoutBlocks = [regex]::Replace(
        $Text,
        '/\*.*?\*/',
        '',
        [Text.RegularExpressions.RegexOptions]::Singleline)
    return [regex]::Replace(
        $withoutBlocks,
        '//.*?$',
        '',
        [Text.RegularExpressions.RegexOptions]::Multiline)
}

function Get-LeafTypeName {
    param([string]$TypeName)
    if ([string]::IsNullOrWhiteSpace($TypeName)) { return '' }
    $parts = $TypeName -split '::'
    return $parts[$parts.Count - 1]
}

function Test-InheritanceBoundaries {
    <#
    .SYNOPSIS
    只读检查 GamePlatform -> MobaCommon -> DivineBeasts 的源码继承和 Public API 边界。
    .DESCRIPTION
    本检查只分析 Source 下受版本管理的 C++ 头文件，不解析 UE 二进制资产。
    Blueprint/DataAsset 真实父类仍须由 UE AssetRegistry/DataValidation 在有真实资产时验证。
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$WorkspaceRoot)

    $workspace = [IO.Path]::GetFullPath($WorkspaceRoot)
    $pluginRoot = Join-Path $workspace 'Game/Plugins'
    $findings = New-Object 'System.Collections.Generic.List[string]'
    if (-not (Test-Path -LiteralPath $pluginRoot -PathType Container)) {
        return [pscustomobject]@{
            Passed=$false; Findings=@('缺少 Game/Plugins 插件根目录。')
            PublicHeaderCount=0; TypeCount=0; InheritanceEdgeCount=0
        }
    }

    $headers = @(Get-ChildItem -LiteralPath $pluginRoot -Recurse -File -Filter '*.h' |
        Where-Object {
            $_.FullName -match '[\\/]Source[\\/]' -and
            $_.FullName -notmatch '[\\/]Intermediate[\\/]' -and
            $_.FullName -notmatch '[\\/]Docs[\\/]Legacy[\\/]'
        })

    $records = New-Object 'System.Collections.Generic.List[object]'
    $typeOwners = @{}
    $cleanByPath = @{}
    $declarationPattern = '(?ms)\b(?:class|struct)\s+(?:[A-Za-z_][A-Za-z0-9_]*_API\s+)?(?<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:final\s*)?(?::\s*public\s+(?<base>[A-Za-z_][A-Za-z0-9_:]*))?\s*\{'

    foreach ($header in $headers) {
        $layer = Get-InheritanceLayerInfo -Path $header.FullName -PluginRoot $pluginRoot
        if ($null -eq $layer) { continue }

        $source = Get-Content -LiteralPath $header.FullName -Raw -Encoding UTF8
        $clean = Remove-CppComments -Text $source
        $cleanByPath[$header.FullName] = $clean
        $isPublic = $header.FullName -match '[\\/]Source[\\/][^\\/]+[\\/]Public[\\/]'

        foreach ($match in [regex]::Matches($clean, $declarationPattern)) {
            $name = $match.Groups['name'].Value
            if ([string]::IsNullOrWhiteSpace($name)) { continue }
            $baseName = Get-LeafTypeName -TypeName $match.Groups['base'].Value
            $record = [pscustomobject]@{
                Name=$name
                BaseName=$baseName
                LayerName=$layer.Name
                LayerIndex=$layer.Index
                Path=$header.FullName
                IsPublic=$isPublic
            }
            $records.Add($record)
            if (-not $typeOwners.ContainsKey($name)) {
                $typeOwners[$name] = $record
            }
            elseif ($typeOwners[$name].Path -ne $header.FullName) {
                $existing = $typeOwners[$name]
                if ($existing.IsPublic -or $record.IsPublic) {
                    $findings.Add("重复公开类型身份：$name -> $($existing.Path) / $($header.FullName)")
                }
                # Private辅助类型允许在不同模块/命名空间复用短名；Public类型优先用于跨层解析。
                if (-not $existing.IsPublic -and $record.IsPublic) {
                    $typeOwners[$name] = $record
                }
            }
        }
    }

    $edgeCount = 0
    foreach ($record in $records) {
        if ([string]::IsNullOrWhiteSpace($record.BaseName) -or -not $typeOwners.ContainsKey($record.BaseName)) { continue }
        $edgeCount++
        $base = $typeOwners[$record.BaseName]

        if ($record.LayerIndex -lt $base.LayerIndex) {
            $findings.Add(
                "反向继承：$($record.Name)($($record.LayerName)) -> $($base.Name)($($base.LayerName)) [$($record.Path)]")
        }
        if ($record.LayerIndex -gt $base.LayerIndex -and -not $base.IsPublic) {
            $findings.Add(
                "跨层继承Private类型：$($record.Name) -> $($base.Name) [$($base.Path)]")
        }
    }

    $publicHeaders = @($headers | Where-Object { $_.FullName -match '[\\/]Source[\\/][^\\/]+[\\/]Public[\\/]' })
    $publicHigherLayerTypes = @($records | Where-Object { $_.IsPublic })
    foreach ($header in $publicHeaders) {
        $layer = Get-InheritanceLayerInfo -Path $header.FullName -PluginRoot $pluginRoot
        if ($null -eq $layer -or $layer.Index -ge 2) { continue }
        $clean = [string]$cleanByPath[$header.FullName]

        foreach ($candidate in $publicHigherLayerTypes) {
            if ($candidate.LayerIndex -le $layer.Index) { continue }
            if ([regex]::IsMatch($clean, ('\b{0}\b' -f [regex]::Escape($candidate.Name)))) {
                $findings.Add(
                    "低层Public API引用上层类型：$($layer.Name)头文件 $($header.FullName) -> $($candidate.Name)($($candidate.LayerName))")
            }
        }
    }

    $unique = @($findings.ToArray() | Select-Object -Unique)
    return [pscustomobject]@{
        Passed=($unique.Count -eq 0)
        Findings=$unique
        PublicHeaderCount=$publicHeaders.Count
        TypeCount=$records.Count
        InheritanceEdgeCount=$edgeCount
    }
}

Export-ModuleMember -Function Test-InheritanceBoundaries
