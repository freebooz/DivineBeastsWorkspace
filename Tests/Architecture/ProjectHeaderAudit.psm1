Set-StrictMode -Version Latest

function Get-CppLiteralIncludes {
    # 按C++预处理顺序先拼接反斜线续行，再按最先出现的词法单元屏蔽注释／字面量。
    # 不求值宏和#if；只返回真实预处理指令的字面量头及原始物理行号。
    param([string]$Text)
    if ([string]::IsNullOrEmpty($Text)) { return }
    $splices = @([regex]::Matches($Text, '\\\r?\n'))
    $logical = [regex]::Replace($Text, '\\\r?\n', '')
    $pattern = @'
(?<comment>//[^\r\n]*|/\*[\s\S]*?(?:\*/|$))|(?<raw>(?:u8|u|U|L)?R"(?<delimiter>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)\k<delimiter>")|(?<quoted>"(?:\\[\s\S]|[^"\\])*")|(?<character>'(?:\\[\s\S]|[^'\\])*')
'@
    $visible = [Text.StringBuilder]::new()
    $cursor = 0
    foreach ($token in [regex]::Matches($logical, $pattern)) {
        $null = $visible.Append($logical.Substring($cursor, $token.Index - $cursor))
        $isIncludeArgument = $false
        if ($token.Groups['quoted'].Success) {
            $prefix = $visible.ToString()
            $lineStart = $prefix.LastIndexOf("`n") + 1
            $isIncludeArgument = $prefix.Substring($lineStart) -match '^\s*#\s*include\s*$'
        }
        # include中的引号是头名称分隔符；普通/字符/原始字符串中的指令文本不参与扫描。
        $value = if ($isIncludeArgument) { $token.Value } else { [regex]::Replace($token.Value, '[^\r\n]', ' ') }
        $null = $visible.Append($value)
        $cursor = $token.Index + $token.Length
    }
    $null = $visible.Append($logical.Substring($cursor))
    foreach ($includeMatch in [regex]::Matches($visible.ToString(), '(?m)^[^\S\r\n]*#[^\S\r\n]*include[^\S\r\n]*["<](?<header>[^">\r\n]+)[">]')) {
        # 屏蔽操作保持字符数；只有续行删除改变偏移，按原文位置反算诊断行号。
        $originalIndex = $includeMatch.Index
        $removed = 0
        foreach ($splice in $splices) {
            $logicalIndex = $splice.Index - $removed
            if ($logicalIndex -gt $includeMatch.Index) { break }
            $originalIndex += $splice.Length
            $removed += $splice.Length
        }
        [pscustomobject]@{
            Include=$includeMatch.Groups['header'].Value
            Line=([regex]::Matches($Text.Substring(0,$originalIndex), '\n').Count + 1)
        }
    }
}

function Test-ProjectOwnedHeaders {
    <#
    .SYNOPSIS
    检查项目层C++消费者引用的自有头文件是否真实存在，补充插件描述审计。
    .DESCRIPTION
    只读扫描DivineBeasts层Source及Game/Source，索引全部插件／主工程Source和Shared生成头。
    只检查GamePlatform、DivineBeasts、DBA、MobaPresentation的字面量include；UHT的
    .generated.h由引擎生成，不要求预先存在；Shared的.generated.hpp必须存在。
    返回Passed、CheckedIncludes及含源码路径／行号／include的Findings。
    这是缺失文件预检，不解析宏、Build.cs搜索路径、类型签名或目标条件；同名文件
    存在不证明可访问或可编译。缺扫描目录、无源码、零有效引用或读取失败直接抛错。
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory=$true)][string]$WorkspaceRoot)

    $root = [IO.Path]::GetFullPath($WorkspaceRoot)
    $plugins = Join-Path $root 'Game/Plugins'
    $projectPlugins = Join-Path $plugins 'DivineBeasts'
    if (-not (Test-Path -LiteralPath $projectPlugins -PathType Container)) {
        throw "缺少项目插件源码根：$projectPlugins"
    }
    $mainSource = Join-Path $root 'Game/Source'
    $consumerRoots = @($projectPlugins)
    if (Test-Path -LiteralPath $mainSource -PathType Container) { $consumerRoots += $mainSource }
    $sources = @(Get-ChildItem -LiteralPath $consumerRoots -Recurse -File -ErrorAction Stop |
        Where-Object { $_.FullName -match '[\\/]Source[\\/]' -and $_.Extension -in @('.cpp','.h','.hpp') } |
        Sort-Object FullName)
    if ($sources.Count -eq 0) { throw "项目插件目录中没有可审计的C++源码：$projectPlugins" }

    $headerNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $indexRoots = @($plugins)
    if (Test-Path -LiteralPath $mainSource -PathType Container) { $indexRoots += $mainSource }
    foreach ($file in (Get-ChildItem -LiteralPath $indexRoots -Recurse -File -ErrorAction Stop |
        Where-Object { $_.FullName -match '[\\/]Source[\\/]' -and $_.Extension -in @('.h','.hpp') })) {
        $null = $headerNames.Add($file.Name)
    }
    $generated = Join-Path $root 'Shared/Generated/Cpp'
    if (Test-Path -LiteralPath $generated -PathType Container) {
        foreach ($file in (Get-ChildItem -LiteralPath $generated -Recurse -File -ErrorAction Stop |
            Where-Object { $_.Extension -in @('.h','.hpp') })) {
            $null = $headerNames.Add($file.Name)
        }
    }

    $findings = [Collections.Generic.List[object]]::new()
    $checkedIncludes = 0
    foreach ($file in $sources) {
        $text = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction Stop
        foreach ($reference in @(Get-CppLiteralIncludes -Text $text)) {
            $include = $reference.Include
            $leaf = [IO.Path]::GetFileName($include.Replace('\','/'))
            if ($leaf -notmatch '^[AFIEU]?(GamePlatform|DivineBeasts|DBA|MobaPresentation)' -or $leaf -match '\.generated\.h$') { continue }
            $checkedIncludes++
            if (-not $headerNames.Contains($leaf)) {
                $findings.Add([pscustomobject]@{ File=$file.FullName; Line=$reference.Line; Include=$include })
            }
        }
    }
    if ($checkedIncludes -eq 0) { throw "没有有效的自有头文件引用，预检未完成：$root" }
    [pscustomobject]@{
        Passed=($findings.Count -eq 0)
        CheckedIncludes=$checkedIncludes
        Findings=@($findings.ToArray())
    }
}

Export-ModuleMember -Function Test-ProjectOwnedHeaders
