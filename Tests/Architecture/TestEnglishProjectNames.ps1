#requires -Version 7.0
<#
.SYNOPSIS
只读审查工作空间英文路径与活动内容插件的英文显示名称。
.DESCRIPTION
目录及非文档文件名只能使用ASCII字符；文档文件名允许中文，文档目录仍使用英文。
检查隐藏目录，排除Git内部存储；活动Game插件的FriendlyName按同一规则检查。
不重命名、不修改资产；可输出不含凭据的JSON报告。退出0表示命名检查通过，1表示发现违规。
该检查只验证命名，不代替资源引用、编译、Cook或客户端运行验收。
#>
[CmdletBinding()]
param(
    [string]$WorkspaceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')),
    [string]$ReportPath
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $WorkspaceRoot).Path.TrimEnd('\','/')
# 文档名称例外只豁免文件本身，不豁免其祖先目录；配置与代码不在例外集合中。
$documentExtensions = @('.md','.txt','.rst','.pdf','.doc','.docx','.odt','.ppt','.pptx','.xls','.xlsx')
$violations = [Collections.Generic.List[object]]::new()
$items = @(Get-ChildItem -LiteralPath $root -Recurse -Force -ErrorAction Stop |
    Where-Object { $_.FullName -notmatch '[\\/]\.git([\\/]|$)' })
foreach ($item in $items) {
    if ($item.Name -notmatch '[^\x00-\x7f]') { continue }
    if (-not $item.PSIsContainer -and $documentExtensions -contains $item.Extension.ToLowerInvariant()) { continue }
    $violations.Add([pscustomobject]@{
        Kind = if ($item.PSIsContainer) {'DirectoryName'} else {'FileName'}
        Path = $item.FullName.Substring($root.Length+1)
        Reason = '目录或非文档文件名含非ASCII字符。'
    })
}
# 只读取正式插件描述；Saved打包快照和历史源快照不参与活动挂载点显示名称判断。
$plugins = @(Get-ChildItem -LiteralPath (Join-Path $root 'Game/Plugins') -Recurse -File -Filter '*.uplugin')
foreach ($plugin in $plugins) {
    $descriptor = Get-Content -LiteralPath $plugin.FullName -Raw | ConvertFrom-Json
    if ($descriptor.FriendlyName -match '[^\x00-\x7f]') {
        $violations.Add([pscustomobject]@{
            Kind='PluginFriendlyName'; Path=$plugin.FullName.Substring($root.Length+1)
            Reason='活动插件的内容浏览器显示名称含非ASCII字符。'
        })
    }
}
$result = [ordered]@{ Passed=($violations.Count -eq 0); ItemsChecked=$items.Count;
    PluginsChecked=$plugins.Count; ViolationCount=$violations.Count; Violations=@($violations.ToArray()) }
$json = $result | ConvertTo-Json -Depth 5
if ($ReportPath) {
    # 报告仅写调用者明确指定的位置；不改动被审查的文件或目录。
    $parent = Split-Path -Parent ([IO.Path]::GetFullPath($ReportPath))
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) { throw '报告父目录不存在。' }
    $json | Set-Content -LiteralPath $ReportPath -Encoding utf8
}
$json
if ($violations.Count -gt 0) { exit 1 }
exit 0
