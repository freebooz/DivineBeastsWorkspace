Set-StrictMode -Version Latest

function Get-CompositionTargetRuleSource {
    <# 对字面量依赖的只读目标预处理：只求值完整的Target.Type ==/!= TargetType枚举if块。
       不执行C#或UBT；未知表达式/else仍保守保留全部依赖。插值/原始字符串或预处理指令整源保留，
       防止有限词法掩码误吞块外依赖。结构扫描忽略普通注释/字符串中的花括号，
       保持字符位置后仅擦除确定不属于当前目标的块，不按模块名称或期望通过结果做特殊白名单。 #>
    param([string]$Source, [string]$Target)
    # 插值表达式允许嵌套字符串/花括号；原始字符串及预处理可能改变词法/结构，不能用普通字符串正则猜。
    # 即使该语法只在注释中出现也保守不剪枝；代价仅为可能误报，不得漏掉真实客户端泄漏。
    if ($Source -match '(?:\$+@?|@\$+)"|"{3}' -or $Source -match '(?m)^\s*#') { return $Source }
    # C#普通/逐字字符串和字符常量均可能含花括号；它们只用于结构掩码，不参与条件识别。
    $literalPattern = '(?s)//[^\r\n]*|/\*.*?\*/|@"(?:[^"]|"")*"|"(?:\\.|[^"\\])*"|''(?:\\.|[^''\\])*'''
    $structure = [regex]::Replace($Source, $literalPattern, [Text.RegularExpressions.MatchEvaluator]{
        param($literal) return (' ' * $literal.Length)
    })
    $characters = $Source.ToCharArray()
    $conditionPattern = '\bif\s*\(\s*Target\.Type\s*(?<operator>==|!=)\s*TargetType\.(?<type>Editor|Client|Server|Game|Program)\s*\)\s*\{'
    foreach ($condition in [regex]::Matches($structure, $conditionPattern)) {
        $open = $condition.Index + $condition.Length - 1
        $depth = 1; $end = $open + 1
        while ($end -lt $structure.Length -and $depth -gt 0) {
            if ($structure[$end] -eq '{') { $depth++ }
            elseif ($structure[$end] -eq '}') { $depth-- }
            $end++
        }
        # 非平衡或else语法不属于此限定求值器，仍让原审计发现全部字面量依赖而非猜测。
        if ($depth -ne 0 -or $structure.Substring($end) -match '^\s*else\b') { continue }
        $isActive = $Target -eq $condition.Groups['type'].Value
        if ($condition.Groups['operator'].Value -eq '!=') { $isActive = -not $isActive }
        if ($isActive) { continue }
        for ($index = $condition.Index; $index -lt $end; $index++) {
            if ($characters[$index] -ne "`r" -and $characters[$index] -ne "`n") { $characters[$index] = ' ' }
        }
    }
    return (-join $characters)
}

function Test-PluginComposition {
    <#
    .SYNOPSIS
    只读计算指定目标的插件／模块声明闭包，验证可选竞技与端侧边界。
    .DESCRIPTION
    读取.uplugin及Build.cs中的字面量依赖；仅过滤明确单Target.Type条件块，未知条件保守检查。
    不执行UBT、不分析资产或运行时加载，不等价于完整C#规则求值。
    返回Passed、Errors、ReachablePlugins和ReachableModules；缺失、禁用、循环、反向或未声明依赖均失败。
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$WorkspaceRoot,
        [Parameter(Mandatory)][string[]]$RootPlugins,
        [ValidateSet('Client','Server','Editor')][string]$Target = 'Editor',
        [string[]]$DisabledPlugins = @()
    )
    $errors = New-Object 'System.Collections.Generic.List[string]'
    $plugins = @{}; $moduleOwners = @{}
    $pluginRoot = [IO.Path]::GetFullPath((Join-Path $WorkspaceRoot 'Game/Plugins'))
    if (-not (Test-Path -LiteralPath $pluginRoot -PathType Container)) {
        return [pscustomobject]@{Passed=$false;Errors=@('缺少插件根目录');ReachablePlugins=@();ReachableModules=@()}
    }

    # 目标过滤同时检查允许／禁止列表和模块宿主，不能从模块名称后缀猜端侧。
    $supportsTarget = {
        param($Definition, [bool]$IsModule)
        foreach ($propertyName in @('TargetAllowList','WhitelistTargets')) {
            if ($Definition.PSObject.Properties[$propertyName] -and @($Definition.$propertyName).Count -gt 0 -and $Target -notin $Definition.$propertyName) { return $false }
        }
        foreach ($propertyName in @('TargetDenyList','BlacklistTargets')) {
            if ($Definition.PSObject.Properties[$propertyName] -and $Target -in $Definition.$propertyName) { return $false }
        }
        if ($IsModule) {
            $type = [string]$Definition.Type
            if ($type -match '^Editor' -and $Target -ne 'Editor') { return $false }
            if ($type -match '^ClientOnly' -and $Target -eq 'Server') { return $false }
            if ($type -eq 'ServerOnly' -and $Target -eq 'Client') { return $false }
            if ($type -eq 'Program') { return $false }
        }
        return $true
    }
    foreach ($file in Get-ChildItem -LiteralPath $pluginRoot -Filter '*.uplugin' -File -Recurse) {
        try { $descriptor = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop }
        catch { $errors.Add("插件描述无法解析：$($file.FullName)"); continue }
        $name = $file.BaseName
        if ($plugins.ContainsKey($name)) { $errors.Add("重复插件身份：$name"); continue }
        $relative = $file.FullName.Substring($pluginRoot.Length).TrimStart('\','/') -replace '\\','/'
        $layer = if ($relative.StartsWith('GamePlatform/')) { 0 } elseif ($relative.StartsWith('MobaCommon/')) { 1 } else { 2 }
        $plugins[$name] = @{ Descriptor=$descriptor; Directory=$file.DirectoryName; Layer=$layer }
        if ($descriptor.PSObject.Properties['Modules']) {
            foreach ($module in @($descriptor.Modules)) {
                if ($moduleOwners.ContainsKey($module.Name)) { $errors.Add("重复模块身份：$($module.Name)"); continue }
                $moduleOwners[$module.Name] = $name
            }
        }
    }

    $states = @{}; $reachable = @{}; $availableModules = @{}
    $visit = {
        param([string]$Name, [string[]]$Chain)
        $path = @($Chain) + $Name
        if ($Name -in $DisabledPlugins) { $errors.Add("依赖被禁用：$($path -join ' -> ')"); return }
        if (-not $plugins.ContainsKey($Name)) { $errors.Add("缺少本地插件：$($path -join ' -> ')"); return }
        if ($states.ContainsKey($Name)) {
            if ($states[$Name] -eq 1) { $errors.Add("插件循环依赖：$($path -join ' -> ')") }
            return
        }
        $states[$Name] = 1
        $reachable[$Name] = $true
        $entry = $plugins[$Name]
        $descriptor = $entry.Descriptor
        if ($descriptor.PSObject.Properties['Modules']) {
            foreach ($module in @($descriptor.Modules)) {
                if (& $supportsTarget $module $true) { $availableModules[$module.Name] = $Name }
            }
        }
        if ($descriptor.PSObject.Properties['Plugins']) {
            # 插件身份在描述文件中只能出现一次；目标过滤前核对以阻止跨Target拆分重复声明。
            $seenPluginDependencies = @{}
            foreach ($dependency in @($descriptor.Plugins)) {
                $dependencyName = [string]$dependency.Name
                if ($seenPluginDependencies.ContainsKey($dependencyName)) { $errors.Add("重复插件依赖：$Name -> $dependencyName"); continue }
                $seenPluginDependencies[$dependencyName] = $true
                if (($dependency.PSObject.Properties['Enabled'] -and -not $dependency.Enabled) -or -not (& $supportsTarget $dependency $false)) { continue }
                if ($plugins.ContainsKey($dependencyName)) {
                    if ($plugins[$dependencyName].Layer -gt $entry.Layer) { $errors.Add("跨层反向依赖：$Name -> $dependencyName") }
                    & $visit $dependencyName $path
                }
                elseif ($dependencyName -match '^(GamePlatform|DivineBeasts|DBA|MobaPresentation)') {
                    if (-not ($dependency.PSObject.Properties['Optional'] -and $dependency.Optional)) { $errors.Add("缺少本地插件：$($path -join ' -> ') -> $dependencyName") }
                }
            }
        }
        $states[$Name] = 2
    }
    foreach ($rootName in $RootPlugins) { & $visit $rootName @() }

    $moduleEdges = @{}
    foreach ($moduleName in @($availableModules.Keys)) {
        $moduleEdges[$moduleName] = New-Object 'System.Collections.Generic.List[string]'
        $owner = $availableModules[$moduleName]
        $rulePath = Join-Path $plugins[$owner].Directory "Source/$moduleName/$moduleName.Build.cs"
        if (-not (Test-Path -LiteralPath $rulePath -PathType Leaf)) { $errors.Add("缺少模块构建规则：$moduleName"); continue }
        $source = Get-Content -LiteralPath $rulePath -Raw -Encoding UTF8
        $source = Get-CompositionTargetRuleSource -Source $source -Target $Target
        $pattern = '(?:Public|Private|DynamicallyLoaded)DependencyModuleNames\.Add(?:Range)?\s*\((?<body>.*?)\)\s*;'
        $declaredPlugins = @()
        $descriptor = $plugins[$owner].Descriptor
        if ($descriptor.PSObject.Properties['Plugins']) {
            $declaredPlugins = @($descriptor.Plugins | Where-Object {
                (-not $_.PSObject.Properties['Enabled'] -or $_.Enabled) -and (& $supportsTarget $_ $false)
            } | ForEach-Object { $_.Name })
        }
        # 预处理后对同一目标的全部Public/Private/Dynamic声明去重，避免重复链接且不漏引擎模块。
        $seenModuleDependencies = @{}
        foreach ($block in [regex]::Matches($source,$pattern,[Text.RegularExpressions.RegexOptions]::Singleline)) {
            foreach ($match in [regex]::Matches($block.Groups['body'].Value,'"([A-Za-z0-9_]+)"')) {
                $dependency = $match.Groups[1].Value
                if ($seenModuleDependencies.ContainsKey($dependency)) { $errors.Add("重复模块依赖：$moduleName -> $dependency"); continue }
                $seenModuleDependencies[$dependency] = $true
                if (-not $moduleOwners.ContainsKey($dependency)) {
                    if ($dependency -match '^(GamePlatform|DivineBeasts|DBA|MobaPresentation)') { $errors.Add("缺少本地模块：$moduleName -> $dependency") }
                    continue
                }
                $dependencyOwner = $moduleOwners[$dependency]
                if ($dependencyOwner -ne $owner -and $dependencyOwner -notin $declaredPlugins) { $errors.Add("模块跨插件依赖未声明：$owner/$moduleName -> $dependencyOwner/$dependency") }
                if (-not $availableModules.ContainsKey($dependency)) { $errors.Add("目标$Target 模块不可用：$moduleName -> $dependency") }
                else { $moduleEdges[$moduleName].Add($dependency) }
            }
        }
    }
    # 插件图无环不能证明插件内部的模块图无环；同样检查自引用及间接回边。
    $moduleStates = @{}
    $visitModule = {
        param([string]$Name, [string[]]$Chain)
        $path = @($Chain) + $Name
        if ($moduleStates.ContainsKey($Name)) {
            if ($moduleStates[$Name] -eq 1) { $errors.Add("模块循环依赖：$($path -join ' -> ')") }
            return
        }
        $moduleStates[$Name] = 1
        foreach ($dependency in $moduleEdges[$Name]) { & $visitModule $dependency $path }
        $moduleStates[$Name] = 2
    }
    foreach ($moduleName in @($availableModules.Keys)) { & $visitModule $moduleName @() }
    [pscustomobject]@{
        Passed=($errors.Count -eq 0); Target=$Target; Errors=@($errors.ToArray() | Select-Object -Unique)
        ReachablePlugins=@($reachable.Keys | Sort-Object); ReachableModules=@($availableModules.Keys | Sort-Object)
    }
}

Export-ModuleMember -Function Test-PluginComposition
