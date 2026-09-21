#requires -Version 7.0
Set-StrictMode -Version Latest

function Get-OnlineVerificationVerdict {
    <# 完整验收不允许缺项或未知状态当成功；失败优先于未执行。不适用只能由调用方带理由明确登记。 #>
    param([Parameter(Mandatory)][object[]]$Checks,[Parameter(Mandatory)][string[]]$RequiredNames)
    $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $failed=$false;$incomplete=$false
    foreach ($check in $Checks) {
        if (-not $names.Add([string]$check.Name)) { throw "重复验收身份：$($check.Name)" }
        if ($check.Status -notin @('Passed','Failed','NotExecuted','NotApplicable')) { throw "无效验收状态：$($check.Name)" }
        if ($check.Status -eq 'Failed') { $failed=$true }
        if ($check.Status -eq 'NotExecuted') { $incomplete=$true }
        if ($check.Status -eq 'NotApplicable' -and -not $check.Reason) { throw '不适用项必须提供确切理由。' }
        if ($check.Status -eq 'Passed' -and ([int]$check.ExitCode -ne 0 -or -not $check.Evidence)) {
            throw '通过项必须有退出码0和实际证据路径。'
        }
    }
    foreach ($required in $RequiredNames) { if (-not $names.Contains($required)) { $incomplete=$true } }
    if ($failed) { return 1 }
    if ($incomplete) { return 2 }
    return 0
}

function Read-OnlineRunEvidence {
    <# 只接纳本次RunId、工作空间Saved目录内的真实结果；不信任缺RunId、旧日志或目录逃逸。 #>
    param([Parameter(Mandatory)][string]$Path,[Parameter(Mandatory)][string]$Workspace,[Parameter(Mandatory)][guid]$RunId)
    $root=[IO.Path]::GetFullPath((Join-Path $Workspace 'Saved/Validation/GamePlatformOnline'))+[IO.Path]::DirectorySeparatorChar
    $full=[IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($root,[StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $full -PathType Leaf)) { throw '证据不在本工作空间Online验证输出目录。' }
    # 拒绝重解析点，包括祖先目录；不通过链接读取其他任务或外部旧证据。
    $item=Get-Item -LiteralPath $full
    for ($cursor=$item;$null -ne $cursor -and $cursor.FullName.Length -ge $root.TrimEnd('\','/').Length;
        $cursor=if ($cursor -is [IO.FileInfo]) {$cursor.Directory} else {$cursor.Parent}) {
        if ($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw '证据路径含链接，拒绝接纳。' }
    }
    $report=Get-Content -LiteralPath $full -Raw | ConvertFrom-Json
    if (-not $report.PSObject.Properties['RunId'] -or [string]$report.RunId -ne $RunId.ToString('D')) { throw '证据RunId不属于本次运行。' }
    return $report
}

Export-ModuleMember -Function Get-OnlineVerificationVerdict,Read-OnlineRunEvidence
