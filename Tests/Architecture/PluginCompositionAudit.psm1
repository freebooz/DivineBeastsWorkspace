Set-StrictMode -Version Latest

function Test-PluginComposition {
    <#
    .SYNOPSIS
    只读计算指定目标的插件／模块声明闭包，验证可选竞技与端侧边界。
    .DESCRIPTION
    读取.uplugin及Build.cs中的字面量依赖；不执行UBT、不分析资产或运行时加载。
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
            foreach ($dependency in @($descriptor.Plugins)) {
                if (($dependency.PSObject.Properties['Enabled'] -and -not $dependency.Enabled) -or -not (& $supportsTarget $dependency $false)) { continue }
                $dependencyName = [string]$dependency.Name
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
        $pattern = '(?:Public|Private|DynamicallyLoaded)DependencyModuleNames\.Add(?:Range)?\s*\((?<body>.*?)\)\s*;'
        $declaredPlugins = @()
        $descriptor = $plugins[$owner].Descriptor
        if ($descriptor.PSObject.Properties['Plugins']) {
            $declaredPlugins = @($descriptor.Plugins | Where-Object {
                (-not $_.PSObject.Properties['Enabled'] -or $_.Enabled) -and (& $supportsTarget $_ $false)
            } | ForEach-Object { $_.Name })
        }
        foreach ($block in [regex]::Matches($source,$pattern,[Text.RegularExpressions.RegexOptions]::Singleline)) {
            foreach ($match in [regex]::Matches($block.Groups['body'].Value,'"([A-Za-z0-9_]+)"')) {
                $dependency = $match.Groups[1].Value
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
