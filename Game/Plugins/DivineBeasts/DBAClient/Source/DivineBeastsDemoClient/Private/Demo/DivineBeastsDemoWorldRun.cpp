// SSLM世界开发案例执行：唯一可写运行、单次准备/清理超时、完整Data租约及事件快照。
// 只使用注册公开适配；世界退出先失效Token，析构取消本实例需求，不访问下一世界。
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "Demo/DivineBeastsDemoRunState.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "API/IDivineBeastsDemoScenarioAdapter.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"
#include "Async/Async.h"
#include "Evidence/DivineBeastsDemoEvidenceWriter.h"

/** 世界私有执行所有权；适配持共享引用保障同步完成重入时本次调用栈不悬空。 */
struct FDivineBeastsDemoWorldRunPrivateState
{
    DivineBeasts::Demo::FRunState State;
    TSharedPtr<IDivineBeastsDemoScenarioAdapter> Adapter;
    FDivineBeastsDemoScenarioDefinition Scenario;
    FGamePlatformDataLease ResourceLease;
    FTimerHandle DeadlineTimer;
    FGuid WorldId = FGuid::NewGuid();
    uint64 Generation = 0;
    bool bClosing = false;
    FString SourceFingerprint;
    TArray<FDivineBeastsDemoReviewCaseDefinition> ReviewCases;
    TArray<FDivineBeastsDemoFeatureEntry> Features;
    TArray<FString> EventRecords;
    bool bTerminalEvidenceAttempted = false;
};

namespace
{
/** 比较完整普通资源租约；完成时RequestState变化不影响身份。 */
bool IsSameScenarioLease(const FGamePlatformDataLease& A, const FGamePlatformDataLease& B)
{
    return A.IsValid() && B.IsValid() && A.ScopeId == B.ScopeId && A.LeaseId == B.LeaseId &&
        A.Generation == B.Generation && A.IssuerProof == B.IssuerProof && A.DefinitionId == B.DefinitionId &&
        A.Bundles == B.Bundles && A.ResourcePaths == B.ResourcePaths;
}
/** 参数仅在类型合同下解释有效载荷；只读值不能被命令覆盖，数值不得夹紧为假成功。 */
bool ValidateScenarioParameters(const FDivineBeastsDemoScenarioDefinition& Scenario,
    const TArray<FDivineBeastsDemoParameterValue>& Values)
{
    if (Values.Num() != Scenario.Parameters.Num()) return false;
    for (int32 Index = 0; Index < Values.Num(); ++Index)
    {
        const auto& Definition = Scenario.Parameters[Index]; const auto& Value = Values[Index];
        if (Value.Type != Definition.DefaultValue.Type) return false;
        bool bSame = false;
        switch (Value.Type)
        {
        case EDivineBeastsDemoParameterType::Boolean: bSame = Value.bBooleanValue == Definition.DefaultValue.bBooleanValue; break;
        case EDivineBeastsDemoParameterType::Identifier:
            if (Value.IdentifierValue.IsNone()) return false;
            bSame = Value.IdentifierValue == Definition.DefaultValue.IdentifierValue; break;
        case EDivineBeastsDemoParameterType::Integer:
            if (Value.IntegerValue < -9007199254740991LL || Value.IntegerValue > 9007199254740991LL ||
                static_cast<double>(Value.IntegerValue) < Definition.Minimum || static_cast<double>(Value.IntegerValue) > Definition.Maximum) return false;
            bSame = Value.IntegerValue == Definition.DefaultValue.IntegerValue; break;
        case EDivineBeastsDemoParameterType::Number:
            if (!FMath::IsFinite(Value.NumberValue) || Value.NumberValue < Definition.Minimum || Value.NumberValue > Definition.Maximum) return false;
            bSame = Value.NumberValue == Definition.DefaultValue.NumberValue; break;
        default: return false;
        }
        if ((!Definition.bWritable || Definition.ApplyWhen == TEXT("prepare")) && !bSame) return false;
    }
    return true;
}
/** 不合规后台回调只在游戏线程报告失败；不得在后台解引用UObject。 */
void DeliverDemoCompletion(TFunction<void(const FGamePlatformResult&)> Deliver, FGamePlatformResult Result)
{
    if (IsInGameThread()) { Deliver(Result); return; }
    AsyncTask(ENamedThreads::GameThread, [Deliver = MoveTemp(Deliver)]()
    { Deliver(FGamePlatformResult::Failure(FName(TEXT("Demo.WrongCallbackThread")), TEXT("适配器违反游戏线程完成合同。"))); });
}
}

bool UDivineBeastsDemoWorldSubsystem::HasActiveScenario() const { return RunPrivateState && RunPrivateState->State.IsActive(); }
FDivineBeastsDemoRunSnapshot UDivineBeastsDemoWorldSubsystem::GetRunSnapshot() const
{ return RunPrivateState ? RunPrivateState->State.GetSnapshot() : FDivineBeastsDemoRunSnapshot(); }
FName UDivineBeastsDemoWorldSubsystem::GetActiveAdapterId() const
{ return HasActiveScenario() ? RunPrivateState->Scenario.AdapterId : NAME_None; }
void UDivineBeastsDemoWorldSubsystem::PublishRunSnapshot()
{
    const auto Snapshot = GetRunSnapshot();
    if (RunPrivateState && Snapshot.Token.IsValid())
    {
        // 这是诊断记录的内存上限，不是游戏性能预算。达到上限保留标记，不无限累计命令历史。
        if (RunPrivateState->EventRecords.Num() >= 4096) RunPrivateState->EventRecords.RemoveAt(1);
        RunPrivateState->EventRecords.Add(FString::Printf(TEXT("state=%d operation_status=%d operation_code=%s cleanup_status=%d"),
        static_cast<uint8>(Snapshot.State), static_cast<uint8>(Snapshot.OperationResult.Status), *Snapshot.OperationResult.Code.ToString(), static_cast<uint8>(Snapshot.CleanupResult.Status)));
        const bool bTerminal = Snapshot.State == EDivineBeastsDemoRunState::Completed || Snapshot.State == EDivineBeastsDemoRunState::Failed || Snapshot.State == EDivineBeastsDemoRunState::Cancelled;
        if (bTerminal && !RunPrivateState->bTerminalEvidenceAttempted)
        {
            // 在向用户发布终态之前冻结文件，订阅者同步启动新案例也不能覆盖上一场事实。
            RunPrivateState->bTerminalEvidenceAttempted = true;
            FString Path, Error;
            if (!WriteCurrentEvidence(Path, Error)) LastFeedback = FText::FromString(TEXT("运行终态已记录在内存，但证据保存失败：") + Error);
        }
    }
    LastFeedback = FText::FromString(Snapshot.TerminalResult.Status != EGamePlatformResultStatus::NotExecuted
        ? Snapshot.TerminalResult.Message : Snapshot.OperationResult.Message);
    OnRunChanged.Broadcast(Snapshot);
}
bool UDivineBeastsDemoWorldSubsystem::BeginScenario(const FDivineBeastsDemoScenarioDefinition& Scenario)
{
    check(IsInGameThread());
    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown || HasActiveScenario()) return false;
    auto* Catalog = World->GetGameInstance()->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>();
    if (!Catalog || !Catalog->PreflightScenario(Scenario.ScenarioId).bCanStart) return false;
    if (!RunPrivateState) RunPrivateState = MakeShared<FDivineBeastsDemoWorldRunPrivateState>();
    if (RunPrivateState->bClosing || RunPrivateState->Generation == MAX_uint64) return false;
    auto Created = Catalog->CreateScenarioAdapter(Scenario.AdapterId, *World);
    if (!Created) { LastFeedback = FText::FromString(TEXT("已注册适配工厂未能创建本世界实例。")); return false; }
    FDivineBeastsDemoRunToken Token;
    Token.RunId = FGuid::NewGuid(); Token.WorldId = RunPrivateState->WorldId; Token.Generation = ++RunPrivateState->Generation;
    if (!RunPrivateState->State.Begin(Token, Scenario.ScenarioId, Scenario.FeatureId, FPlatformTime::Seconds(), Scenario.PrepareTimeoutSeconds)) return false;
    RunPrivateState->Scenario = Scenario; RunPrivateState->Adapter = MakeShareable(Created.Release());
    RunPrivateState->SourceFingerprint = Catalog->GetSourceFingerprint(); RunPrivateState->ReviewCases.Reset(); RunPrivateState->EventRecords.Reset();
    RunPrivateState->Features.Reset(); RunPrivateState->bTerminalEvidenceAttempted = false;
    for (const auto& Plugin : Catalog->GetPlugins())
        for (const auto& Feature : Catalog->GetFeatures(Plugin.PluginId))
            if (Feature.FeatureId == Scenario.FeatureId || Scenario.RelatedFeatureIds.Contains(Feature.FeatureId)) RunPrivateState->Features.Add(Feature);
    for (const auto& Case : Catalog->GetReviewCases()) if (Scenario.ReviewCaseIds.Contains(Case.ReviewCaseId)) RunPrivateState->ReviewCases.Add(Case);
    ArmScenarioDeadline(); PublishRunSnapshot();
    // 事件可同步取消/切换；返回后必须确认仍是原准备，不能把旧请求租约写进新运行。
    if (!RunPrivateState || RunPrivateState->bClosing || !(RunPrivateState->State.GetSnapshot().Token == Token) ||
        RunPrivateState->State.GetSnapshot().State != EDivineBeastsDemoRunState::Preparing) return true;
    TArray<FSoftObjectPath> Resources = Scenario.DefinitionAssets;
    if (!Scenario.WidgetAsset.IsNull()) Resources.AddUnique(Scenario.WidgetAsset);
    if (Resources.IsEmpty()) { PrepareScenarioAdapter(); return true; }
    auto* Data = IGamePlatformDataService::Get(*World->GetGameInstance());
    if (!Data)
    { OnScenarioPrepared(Token, FGamePlatformResult::Failure(FName(TEXT("Demo.DataUnavailable")), TEXT("准备时中央Data服务不可用。"))); return true; }
    FGamePlatformResult Accepted;
    TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> WeakThis(this);
    RunPrivateState->ResourceLease = Data->AcquireResources(Resources, EGamePlatformDataLifetime::World, this,
        [WeakThis, Token](const auto& Lease, const auto& Result)
        { if (auto* Self = WeakThis.Get()) Self->OnScenarioResourcesReady(Token, Lease, Result); }, Accepted);
    if (!Accepted.IsSuccess()) OnScenarioPrepared(Token, Accepted);
    return true;
}
void UDivineBeastsDemoWorldSubsystem::OnScenarioResourcesReady(const FDivineBeastsDemoRunToken& Token,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    auto* Data = GetWorld() && GetWorld()->GetGameInstance() ? IGamePlatformDataService::Get(*GetWorld()->GetGameInstance()) : nullptr;
    if (!RunPrivateState || RunPrivateState->bClosing || !(RunPrivateState->State.GetSnapshot().Token == Token) ||
        RunPrivateState->State.GetSnapshot().State != EDivineBeastsDemoRunState::Preparing)
    { if (Data && Lease.IsValid()) Data->ReleaseResources(Lease); return; }
    if (!Result.IsSuccess() || !IsSameScenarioLease(Lease, RunPrivateState->ResourceLease))
    { OnScenarioPrepared(Token, Result.IsSuccess() ? FGamePlatformResult::Failure(FName(TEXT("Demo.InvalidResourceLease")), TEXT("样板资源完整租约不匹配。")) : Result); return; }
    if (RunPrivateState->State.GetDeadlineSeconds() <= FPlatformTime::Seconds()) { OnScenarioDeadline(); return; }
    if (!RunPrivateState->Scenario.WidgetAsset.IsNull())
    {
        auto* Class = Cast<UClass>(RunPrivateState->Scenario.WidgetAsset.ResolveObject());
        auto* Player = GetWorld()->GetGameInstance()->GetFirstGamePlayer();
        auto* Controller = Player ? Player->GetPlayerController(GetWorld()) : nullptr;
        if (!Class || !Class->IsChildOf(UUserWidget::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract) || !Controller)
        { OnScenarioPrepared(Token, FGamePlatformResult::Failure(FName(TEXT("Demo.SampleWidgetUnavailable")), TEXT("样板控件类/本地控制器不可用，不能创建真实样板。"))); return; }
        ActiveSampleWidget = CreateWidget<UUserWidget>(Controller, Class);
        if (!ActiveSampleWidget)
        { OnScenarioPrepared(Token, FGamePlatformResult::Failure(FName(TEXT("Demo.SampleWidgetCreateFailed")), TEXT("真实样板控件创建失败。"))); return; }
    }
    PrepareScenarioAdapter();
}
void UDivineBeastsDemoWorldSubsystem::PrepareScenarioAdapter()
{
    if (!RunPrivateState || RunPrivateState->bClosing || !RunPrivateState->Adapter ||
        RunPrivateState->State.GetSnapshot().State != EDivineBeastsDemoRunState::Preparing) return;
    FDivineBeastsDemoScenarioContext Context;
    Context.Token = RunPrivateState->State.GetSnapshot().Token; Context.World = GetWorld();
    Context.Scenario = RunPrivateState->Scenario; Context.PrepareDeadlineSeconds = RunPrivateState->State.GetDeadlineSeconds();
    Context.SampleWidget = ActiveSampleWidget.Get();
    TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> WeakThis(this);
    const auto Adapter = RunPrivateState->Adapter;
    TGuardValue<bool> Writing(bExecutingScenarioAdapter, true);
    Adapter->Prepare(Context, [WeakThis](const auto& Token, const auto& Result)
    { DeliverDemoCompletion([WeakThis, Token](const auto& Delivered) { if (auto* Self = WeakThis.Get()) Self->OnScenarioPrepared(Token, Delivered); }, Result); });
}
void UDivineBeastsDemoWorldSubsystem::OnScenarioPrepared(const FDivineBeastsDemoRunToken& Token, const FGamePlatformResult& Result)
{
    if (!RunPrivateState || !(RunPrivateState->State.GetSnapshot().Token == Token)) return;
    // 计时器与完成可在同帧以任意次序投递；以单调截止裁决，不靠先收到谁决定逾期成功。
    if (RunPrivateState->State.GetSnapshot().State == EDivineBeastsDemoRunState::Preparing &&
        RunPrivateState->State.GetDeadlineSeconds() > 0.0 && RunPrivateState->State.GetDeadlineSeconds() <= FPlatformTime::Seconds())
    { OnScenarioDeadline(); return; }
    if (!RunPrivateState || RunPrivateState->bClosing || !RunPrivateState->State.CompletePreparation(Token, Result)) return;
    PublishRunSnapshot();
    if (!RunPrivateState || !(RunPrivateState->State.GetSnapshot().Token == Token)) return;
    if (!Result.IsSuccess()) RequestScenarioStop(Result);
    else ArmScenarioDeadline();
}
bool UDivineBeastsDemoWorldSubsystem::RequestScenarioOperation(FName Operation, const TArray<FDivineBeastsDemoParameterValue>& Input)
{
    check(IsInGameThread());
    if (Operation == TEXT("Stop")) return RequestScenarioStop(FGamePlatformResult::Success());
    if (!RunPrivateState || RunPrivateState->bClosing || !RunPrivateState->Adapter || !RunPrivateState->Scenario.Operations.Contains(Operation)) return false;
    TArray<FDivineBeastsDemoParameterValue> Values = Input;
    if (Values.IsEmpty()) for (const auto& Parameter : RunPrivateState->Scenario.Parameters) Values.Add(Parameter.DefaultValue);
    if (!ValidateScenarioParameters(RunPrivateState->Scenario, Values))
    { LastFeedback = FText::FromString(TEXT("参数类型、数量、范围、只读或应用阶段不符合案例合同。")); return false; }
    const auto Token = RunPrivateState->State.GetSnapshot().Token;
    uint64 Serial = 0;
    if (!RunPrivateState->State.BeginOperation(Token, Serial)) return false;
    PublishRunSnapshot();
    if (!RunPrivateState || !(RunPrivateState->State.GetSnapshot().Token == Token) ||
        RunPrivateState->State.GetSnapshot().State != EDivineBeastsDemoRunState::Running ||
        !RunPrivateState->State.GetSnapshot().bOperationPending) return true;
    TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> WeakThis(this);
    const auto Adapter = RunPrivateState->Adapter;
    TGuardValue<bool> Writing(bExecutingScenarioAdapter, true);
    Adapter->Execute(Operation, Values, [WeakThis, Serial](const auto& CompletedToken, const auto& Result)
    { DeliverDemoCompletion([WeakThis, Serial, CompletedToken](const auto& Delivered)
        { if (auto* Self = WeakThis.Get()) Self->OnScenarioOperationCompleted(CompletedToken, Serial, Delivered); }, Result); });
    return true;
}
void UDivineBeastsDemoWorldSubsystem::OnScenarioOperationCompleted(const FDivineBeastsDemoRunToken& Token, uint64 Serial, const FGamePlatformResult& Result)
{
    if (RunPrivateState && !RunPrivateState->bClosing && RunPrivateState->State.CompleteOperation(Token, Serial, Result)) PublishRunSnapshot();
}
bool UDivineBeastsDemoWorldSubsystem::RequestScenarioStop(const FGamePlatformResult& Terminal)
{
    check(IsInGameThread());
    if (!RunPrivateState || RunPrivateState->bClosing) return false;
    if (RunPrivateState->State.GetSnapshot().State == EDivineBeastsDemoRunState::Stopping) return true;
    const auto Token = RunPrivateState->State.GetSnapshot().Token;
    if (!RunPrivateState->State.BeginStop(Token, Terminal, FPlatformTime::Seconds(), RunPrivateState->Scenario.StopTimeoutSeconds)) return false;
    ArmScenarioDeadline(); PublishRunSnapshot();
    const auto Adapter = RunPrivateState->Adapter;
    if (!Adapter)
    { OnScenarioStopCompleted(Token, FGamePlatformResult::Failure(FName(TEXT("Demo.AdapterLost")), TEXT("清理时适配实例已失效。"))); return true; }
    TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> WeakThis(this);
    TGuardValue<bool> Writing(bExecutingScenarioAdapter, true);
    Adapter->Stop([WeakThis](const auto& CompletedToken, const auto& Result)
    { DeliverDemoCompletion([WeakThis, CompletedToken](const auto& Delivered)
        { if (auto* Self = WeakThis.Get()) Self->OnScenarioStopCompleted(CompletedToken, Delivered); }, Result); });
    return true;
}
void UDivineBeastsDemoWorldSubsystem::OnScenarioStopCompleted(const FDivineBeastsDemoRunToken& Token, const FGamePlatformResult& Result)
{
    if (!RunPrivateState || RunPrivateState->bClosing || !(RunPrivateState->State.GetSnapshot().Token == Token) ||
        RunPrivateState->State.GetSnapshot().State != EDivineBeastsDemoRunState::Stopping) return;
    if (RunPrivateState->State.GetDeadlineSeconds() <= FPlatformTime::Seconds()) { OnScenarioDeadline(); return; }
    const auto KeepAlive = RunPrivateState->Adapter;
    const auto Released = ReleaseScenarioResources();
    if (!RunPrivateState->State.CompleteStop(Token, Result.IsSuccess() ? Released : Result)) return;
    const auto Completed = GetRunSnapshot();
    RunPrivateState->Adapter.Reset(); ArmScenarioDeadline(); PublishRunSnapshot();
    if (auto* Catalog = GetWorld()->GetGameInstance()->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>())
        Catalog->OnScenarioStopped(*GetWorld(), Completed);
}
void UDivineBeastsDemoWorldSubsystem::ArmScenarioDeadline()
{
    if (!RunPrivateState || !GetWorld()) return;
    GetWorld()->GetTimerManager().ClearTimer(RunPrivateState->DeadlineTimer);
    const double Deadline = RunPrivateState->State.GetDeadlineSeconds();
    if (Deadline <= 0.0) return;
    const float Delay = static_cast<float>(FMath::Clamp(Deadline - FPlatformTime::Seconds(), 0.001, static_cast<double>(MAX_flt)));
    GetWorld()->GetTimerManager().SetTimer(RunPrivateState->DeadlineTimer, this, &ThisClass::OnScenarioDeadline, Delay, false);
}
void UDivineBeastsDemoWorldSubsystem::OnScenarioDeadline()
{
    if (!RunPrivateState || RunPrivateState->bClosing) return;
    const auto Previous = RunPrivateState->State.GetSnapshot().State;
    if (!RunPrivateState->State.Expire(FPlatformTime::Seconds())) { ArmScenarioDeadline(); return; }
    if (Previous == EDivineBeastsDemoRunState::Preparing) RequestScenarioStop(RunPrivateState->State.GetSnapshot().OperationResult);
    else
    {
        RunPrivateState->Adapter.Reset(); ReleaseScenarioResources(); PublishRunSnapshot();
        if (auto* Catalog = GetWorld()->GetGameInstance()->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>())
            Catalog->OnScenarioStopped(*GetWorld(), GetRunSnapshot());
    }
}
FGamePlatformResult UDivineBeastsDemoWorldSubsystem::ReleaseScenarioResources()
{
    if (ActiveSampleWidget) ActiveSampleWidget->RemoveFromParent();
    ActiveSampleWidget = nullptr;
    if (!RunPrivateState) return FGamePlatformResult::Success();
    const auto Lease = RunPrivateState->ResourceLease; RunPrivateState->ResourceLease = {};
    if (Lease.IsValid() && GetWorld() && GetWorld()->GetGameInstance())
        if (auto* Data = IGamePlatformDataService::Get(*GetWorld()->GetGameInstance())) return Data->ReleaseResources(Lease);
    if (Lease.IsValid()) return FGamePlatformResult::Failure(FName(TEXT("Demo.ReleaseServiceUnavailable")), TEXT("释放样板资源时中央服务不可用，未确认资源清理完成。"));
    return FGamePlatformResult::Success();
}
void UDivineBeastsDemoWorldSubsystem::ShutdownScenario()
{
    if (!RunPrivateState) return;
    RunPrivateState->bClosing = true; RunPrivateState->State.AbandonWorld();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RunPrivateState->DeadlineTimer);
    // 先失效Token，再析构受控适配，取消回调无法重新激活运行。
    RunPrivateState->Adapter.Reset(); ReleaseScenarioResources(); PublishRunSnapshot(); OnRunChanged.Clear();
}
bool UDivineBeastsDemoWorldSubsystem::WriteCurrentEvidence(FString& OutPath, FString& OutError)
{
    if (!RunPrivateState) { OutError = TEXT("没有实际运行可导出。"); return false; }
    FDivineBeastsDemoRunEvidence Evidence;
    Evidence.Run = GetRunSnapshot(); Evidence.Scenario = RunPrivateState->Scenario;
    Evidence.SourceFingerprint = RunPrivateState->SourceFingerprint; Evidence.ReviewCases = RunPrivateState->ReviewCases;
    Evidence.Features = RunPrivateState->Features;
    Evidence.EventRecords = RunPrivateState->EventRecords;
    return DivineBeasts::Demo::WriteRunEvidence(Evidence, OutPath, OutError);
}
