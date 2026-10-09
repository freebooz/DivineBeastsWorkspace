# 职责：验证遥测门禁实际扫描器，涵盖误报来源及危险正样例；不是UE网络/遥测运行验收。
$modulePath = Join-Path $PSScriptRoot 'TelemetrySubmitCaptureAudit.psm1'
Import-Module $modulePath -Force
Describe '遥测提交求值顺序扫描' {
    It '独立重试lambda不污染安全提交' {
        $source = 'T->BeginSubmitBatch(Batch, [WeakThis, RetryBatch = MoveTemp(RetryBatch)](Result R) {}); void Retry() { [WeakThis, Batch = MoveTemp(Batch)]() {}; }'
        @(Get-TelemetryUnsafeSubmitCaptures $source).Count | Should Be 0
    }
    It '直接捕获移动同一提交变量失败' {
        @(Get-TelemetryUnsafeSubmitCaptures 'T->BeginSubmitBatch(Batch, [WeakThis, Batch = MoveTemp(Batch)](Result R) {});').Count | Should Be 1
    }
    It '换捕获名字仍不能掩盖危险实参' {
        @(Get-TelemetryUnsafeSubmitCaptures 'T->BeginSubmitBatch(Payload, [Copy = MoveTemp(Payload)]() {});').Count | Should Be 1
    }
    It '回调体内稍后的移动不属于求值冲突' {
        @(Get-TelemetryUnsafeSubmitCaptures 'T->BeginSubmitBatch(Batch, [Batch]() { Later(MoveTemp(Batch)); });').Count | Should Be 0
    }
    It '注释中的危险示意不制造误报' {
        @(Get-TelemetryUnsafeSubmitCaptures '// T->BeginSubmitBatch(Batch, [Batch=MoveTemp(Batch)]() {});').Count | Should Be 0
    }
    It '多个提交调用分别判断且空白布局不影响规则' {
        @(Get-TelemetryUnsafeSubmitCaptures 'T->BeginSubmitBatch(A,[X=MoveTemp(X)](){}); T->BeginSubmitBatch(B, [Y=MoveTemp(B)](){});').Count | Should Be 1
    }
    It '当前真实源码通过完整架构门禁' {
        # 直接执行真实门禁；Pester3的Should Not Throw不能替代外部执行退出码证据。
        & (Join-Path $PSScriptRoot 'TestTelemetryArchitecture.ps1') | Out-Null
    }
}
