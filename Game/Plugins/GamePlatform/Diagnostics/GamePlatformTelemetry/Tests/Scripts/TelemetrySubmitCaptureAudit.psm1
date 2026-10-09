# 职责：遥测架构门禁的C++提交实参/捕获扫描；只验证直接内联Completion的求值顺序合同。
# 输入为源码文本，输出为有问题的调用列表；剔除注释/普通字符串，按括号边界切分实际调用。
# 不把重试函数的独立lambda或Completion函数体内后续Move操作误当作提交时捕获；不是C++编译器。
function Get-TelemetryUnsafeSubmitCaptures {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Source)
    $cleanSource = [regex]::Replace($Source, '(?s)/\*.*?\*/|//[^\r\n]*|"(?:\\.|[^"\\])*"', ' ')
    foreach ($call in [regex]::Matches($cleanSource, '\bBeginSubmitBatch\s*\(')) {
        $depth = 1; $start = $call.Index + $call.Length; $end = $start
        for (; $end -lt $cleanSource.Length -and $depth -gt 0; $end++) {
            if ($cleanSource[$end] -eq '(') { $depth++ }
            elseif ($cleanSource[$end] -eq ')') { $depth-- }
        }
        if ($depth -ne 0) { throw '遥测提交调用括号未闭合，拒绝将不完整源码判为安全。' }
        $arguments = $cleanSource.Substring($start, $end - $start - 1)
        # 仅扫描第二实参的直接lambda捕获列表；函数体后续Move不在实参求值期间执行。
        $parts = [regex]::Match($arguments, '^\s*(?<batch>[A-Za-z_]\w*)\s*,\s*\[(?<capture>[^\]]*)\]')
        if ($parts.Success) {
            $batchPattern = [regex]::Escape($parts.Groups['batch'].Value)
            if ($parts.Groups['capture'].Value -match ('\bMoveTemp\s*\(\s*' + $batchPattern + '\s*\)')) {
                'BeginSubmitBatch(' + $parts.Groups['batch'].Value + ', capture moves same argument)'
            }
        }
    }
}
Export-ModuleMember -Function Get-TelemetryUnsafeSubmitCaptures
