// SSLM开发案例状态机；游戏线程值逻辑，截止由World一次性计时器唤醒，测试注入单调时钟。
#include "Demo/DivineBeastsDemoRunState.h"

namespace DivineBeasts::Demo
{
bool FRunState::IsActive() const
{
    return Snapshot.State == EDivineBeastsDemoRunState::Preparing || Snapshot.State == EDivineBeastsDemoRunState::Running ||
        Snapshot.State == EDivineBeastsDemoRunState::Stopping;
}
bool FRunState::IsCurrent(const FDivineBeastsDemoRunToken& Token) const { return IsActive() && Token.IsValid() && Token == Snapshot.Token; }
bool FRunState::Begin(const FDivineBeastsDemoRunToken& Token, FName ScenarioId, FName FeatureId, double Now, double Timeout)
{
    if (IsActive() || !Token.IsValid() || ScenarioId.IsNone() || FeatureId.IsNone() || !FMath::IsFinite(Now) || Now < 0.0 ||
        !FMath::IsFinite(Timeout) || Timeout <= 0.0 || !FMath::IsFinite(Now + Timeout)) return false;
    Snapshot = {}; Snapshot.Token = Token; Snapshot.ScenarioId = ScenarioId; Snapshot.FeatureId = FeatureId;
    Snapshot.State = EDivineBeastsDemoRunState::Preparing;
    DeadlineSeconds = Now + Timeout; OperationSerial = 0; bPrepareExpired = false; DesiredTerminal = {};
    return true;
}
bool FRunState::CompletePreparation(const FDivineBeastsDemoRunToken& Token, const FGamePlatformResult& Result, double Now)
{
    if (!IsCurrent(Token) || Snapshot.State != EDivineBeastsDemoRunState::Preparing || bPrepareExpired) return false;
    if (!FMath::IsFinite(Now)) return false;
    if (Now >= DeadlineSeconds) { Expire(Now); return false; }
    Snapshot.OperationResult = Result;
    // 失败保持待清理，禁止直接终态丢失资源所有权；成功才开放用户操作。
    if (Result.IsSuccess()) { Snapshot.State = EDivineBeastsDemoRunState::Running; DeadlineSeconds = 0.0; }
    else bPrepareExpired = true; // 抑制同一次准备的重复完成，仍允许BeginStop。
    return true;
}
bool FRunState::BeginOperation(const FDivineBeastsDemoRunToken& Token, uint64& OutSerial)
{
    OutSerial = 0;
    if (!IsCurrent(Token) || Snapshot.State != EDivineBeastsDemoRunState::Running || Snapshot.bOperationPending || OperationSerial == MAX_uint64) return false;
    Snapshot.bOperationPending = true; OutSerial = ++OperationSerial; return true;
}
bool FRunState::CompleteOperation(const FDivineBeastsDemoRunToken& Token, uint64 Serial, const FGamePlatformResult& Result)
{
    if (!IsCurrent(Token) || Snapshot.State != EDivineBeastsDemoRunState::Running || !Snapshot.bOperationPending || Serial != OperationSerial) return false;
    Snapshot.bOperationPending = false; Snapshot.OperationResult = Result;
    if (Snapshot.CompletedOperations < MAX_int64) ++Snapshot.CompletedOperations;
    return true;
}
bool FRunState::BeginStop(const FDivineBeastsDemoRunToken& Token, const FGamePlatformResult& Terminal, double Now, double Timeout)
{
    if (!IsCurrent(Token) || Snapshot.State == EDivineBeastsDemoRunState::Stopping ||
        Terminal.Status == EGamePlatformResultStatus::NotExecuted || !FMath::IsFinite(Now) ||
        !FMath::IsFinite(Timeout) || Timeout <= 0.0 || !FMath::IsFinite(Now + Timeout)) return false;
    if (Snapshot.bOperationPending) Snapshot.OperationResult = FGamePlatformResult::Cancelled(TEXT("操作未完成即停止，迟到回调不计执行完成。"));
    Snapshot.State = EDivineBeastsDemoRunState::Stopping; Snapshot.bOperationPending = false;
    DesiredTerminal = Terminal; DeadlineSeconds = Now + Timeout; return true;
}
bool FRunState::CompleteStop(const FDivineBeastsDemoRunToken& Token, const FGamePlatformResult& Cleanup, double Now)
{
    if (!IsCurrent(Token) || Snapshot.State != EDivineBeastsDemoRunState::Stopping) return false;
    if (!FMath::IsFinite(Now)) return false;
    if (Now >= DeadlineSeconds) { Expire(Now); return false; }
    Snapshot.CleanupResult = Cleanup;
    Snapshot.TerminalResult = Cleanup.IsSuccess() ? DesiredTerminal : Cleanup;
    Snapshot.State = Snapshot.TerminalResult.IsSuccess() ? EDivineBeastsDemoRunState::Completed :
        (Snapshot.TerminalResult.Status == EGamePlatformResultStatus::Cancelled ? EDivineBeastsDemoRunState::Cancelled : EDivineBeastsDemoRunState::Failed);
    DeadlineSeconds = 0.0; return true;
}
bool FRunState::Expire(double Now)
{
    if (!IsActive() || DeadlineSeconds <= 0.0 || !FMath::IsFinite(Now) || Now < DeadlineSeconds) return false;
    if (Snapshot.State == EDivineBeastsDemoRunState::Preparing)
    {
        Snapshot.OperationResult = FGamePlatformResult::Failure(FName(TEXT("Demo.PrepareTimeout")), TEXT("案例准备超时，开始清理本次资源。"));
        bPrepareExpired = true; DeadlineSeconds = 0.0; return true;
    }
    if (Snapshot.State == EDivineBeastsDemoRunState::Stopping)
    {
        Snapshot.CleanupResult = FGamePlatformResult::Failure(FName(TEXT("Demo.StopTimeout")), TEXT("案例清理超时，未确认恢复完成。"));
        Snapshot.TerminalResult = Snapshot.CleanupResult; Snapshot.State = EDivineBeastsDemoRunState::Failed; DeadlineSeconds = 0.0;
        return true;
    }
    return false;
}
void FRunState::AbandonWorld()
{
    if (!IsActive()) return;
    Snapshot.bOperationPending = false; DeadlineSeconds = 0.0; bPrepareExpired = true;
    Snapshot.CleanupResult = FGamePlatformResult::Cancelled(TEXT("世界正在退出，仅撤销本地所有权，不能确认旧世界恢复结果。"));
    Snapshot.TerminalResult = FGamePlatformResult::Cancelled(TEXT("世界退出，当前运行取消。"));
    Snapshot.State = EDivineBeastsDemoRunState::Cancelled;
}
}
