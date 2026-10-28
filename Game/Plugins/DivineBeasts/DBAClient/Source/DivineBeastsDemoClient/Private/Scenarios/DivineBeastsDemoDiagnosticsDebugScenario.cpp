// 诊断人工页使用真实平台注册表；仅产生自有、明确标注的诊断样本，不写玩法权威。
#include "Scenarios/DivineBeastsDemoDiagnosticsDebugScenario.h"
#include "Registry/GamePlatformDebugRegistry.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

namespace DivineBeasts::Demo
{
namespace
{
struct FDebugManualValues
{
    int64 Counter = 1;
    int32 FieldCount = 3;
    int32 ValueLength = 32;
    bool bExpensive = false;
};
class FDebugManualProvider final : public IGamePlatformDebugStateProvider
{
public:
    FName Id;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AActor> Target;
    TSharedRef<FDebugManualValues> Values;
    explicit FDebugManualProvider(TSharedRef<FDebugManualValues> InValues) : Values(MoveTemp(InValues)) {}
    FName GetProviderId() const override { return Id; }
    bool CanCollect(const FGamePlatformDebugCollectContext& Context) const override
    { return World.IsValid() && Target.IsValid() && Context.World == World && Context.Target == Target; }
    EGamePlatformDebugProviderCost GetEstimatedCost() const override
    { return Values->bExpensive ? EGamePlatformDebugProviderCost::Expensive : EGamePlatformDebugProviderCost::Cheap; }
    bool CollectSnapshot(const FGamePlatformDebugCollectContext& Context, FGamePlatformDebugSnapshot& Out) const override
    {
        if (!CanCollect(Context)) return false;
        Out.AddField(TEXT("ManualCounter"), TEXT("人工样本值"), LexToString(Values->Counter), EGamePlatformDebugValueType::Integer);
        // 固定虚构标记，仅用于证明原生敏感字段过滤；不读取任何账号、凭据或网络原文。
        Out.AddField(TEXT("ExplicitPrivate"), TEXT("显式敏感样本"), TEXT("private-fixture-marker"), EGamePlatformDebugValueType::String, EGamePlatformDebugSeverity::Info, true);
        Out.AddField(TEXT("AccessToken"), TEXT("字段名过滤样本"), TEXT("private-fixture-marker"));
        const FString Text = FString::ChrN(Values->ValueLength, TEXT('x'));
        for (int32 Index = 0; Index < Values->FieldCount; ++Index)
            Out.AddField(FName(*FString::Printf(TEXT("ManualField%d"), Index)), TEXT("有界样本字段"), Text);
        return true;
    }
};
struct FDebugManualState
{
    FDivineBeastsDemoScenarioContext Context;
    TSharedRef<FDebugManualValues> Values = MakeShared<FDebugManualValues>();
    TSharedPtr<FDebugManualProvider> Provider;
    TWeakObjectPtr<AActor> Target;
    FName CommandId;
    bool bPrepared = false;
    bool bProviderOwned = false;
    bool bCommandOwned = false;
    bool bCleaning = false;
    bool bSpawningTarget = false;
    FDivineBeastsDemoOperationCompletion DeferredStop;
    uint64 Generation = 0;
    FGamePlatformResult SpawnTarget()
    {
        UWorld* World = Context.World.Get();
        if (!World || World->bIsTearingDown) return FGamePlatformResult::Failure(TEXT("Demo.DebugWorld"), TEXT("诊断样本世界已失效。"));
        if (bSpawningTarget) return FGamePlatformResult::Failure(TEXT("Demo.DebugSpawnReentry"), TEXT("本例目标仍在创建。"));
        bSpawningTarget = true;
        ON_SCOPE_EXIT
        {
            bSpawningTarget = false;
            if (DeferredStop)
            {
                auto Complete = MoveTemp(DeferredStop);
                const auto Result = Clear();
                Complete(Context.Token, Result);
            }
        };
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AActor* Actor = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
        if (!Actor) return FGamePlatformResult::Failure(TEXT("Demo.DebugTarget"), TEXT("自己的弱目标Actor创建失败。"));
        Actor->SetReplicates(false); Actor->SetActorEnableCollision(false); Target = Actor;
        if (Provider) Provider->Target = Target;
        return FGamePlatformResult::Success();
    }
    FGamePlatformResult Clear()
    {
        if (bCleaning) return FGamePlatformResult::Failure(TEXT("Demo.DebugCleaning"), TEXT("诊断样板正在清理，保留原归属。"));
        TGuardValue<bool> Guard(bCleaning, true);
        auto& Registry = FGamePlatformDebugRegistry::Get();
        // 身份包含本Run随机GUID；不注销任何固定平台/项目提供者或控制台执行者。
        if (bProviderOwned && Provider) { Registry.UnregisterStateProvider(Provider->Id); bProviderOwned = false; }
        if (bCommandOwned) { Registry.UnregisterCommandDescriptor(CommandId); bCommandOwned = false; }
        if (AActor* Actor = Target.Get())
        {
            if (!Actor->Destroy()) return FGamePlatformResult::Failure(TEXT("Demo.DebugTargetCleanup"), TEXT("本次Actor未能销毁，保留弱目标以便重试。"));
        }
        Target.Reset(); if (Provider) Provider->Target.Reset();
        Registry.PruneExpiredTargets();
        return FGamePlatformResult::Success();
    }
    void Observe(const FString& Text) const
    {
        if (bPrepared && Context.PublishObservation)
            Context.PublishObservation(Context.Token, Text.Len() <= 3900 ? Text : Text.Left(3800) + TEXT("\n界面摘要已截断；原生快照仍由平台限长。"));
    }
};
class FDebugManualScenario final : public IDivineBeastsDemoScenarioAdapter
{
public:
    TSharedRef<FDebugManualState> State = MakeShared<FDebugManualState>();
    ~FDebugManualScenario() override { State->bPrepared = false; ++State->Generation; State->Clear(); }
    void Prepare(const FDivineBeastsDemoScenarioContext& Context, FDivineBeastsDemoOperationCompletion Complete) override
    {
        if (State->bPrepared || State->bProviderOwned || State->bCommandOwned || State->Target.IsValid() || State->bSpawningTarget || !Context.Token.IsValid() || !Context.World.IsValid() || !Context.World->IsGameWorld() ||
            Context.World->GetNetMode() == NM_DedicatedServer || Context.World->bIsTearingDown || Context.Scenario.AdapterId != TEXT("PlatformDebug") ||
            (Context.Scenario.FeatureId != TEXT("GamePlatformDebug.snapshots") && Context.Scenario.FeatureId != TEXT("GamePlatformDebug.command_catalog")))
        { Complete(Context.Token, FGamePlatformResult::Failure(TEXT("Demo.DebugPrepare"), TEXT("需要当前开发游戏世界、有效身份及已登记的诊断案例。"))); return; }
        State->Context = Context;
        const bool bSnapshots = Context.Scenario.FeatureId == TEXT("GamePlatformDebug.snapshots");
        const TArray<FName> ParameterIds = bSnapshots ? TArray<FName>{TEXT("Counter"), TEXT("FieldCount"), TEXT("ValueLength"), TEXT("AllowExpensive"), TEXT("ExpensiveProvider")} : TArray<FName>{TEXT("MutatingDescriptor")};
        bool bParameterContract = Context.Scenario.Parameters.Num() == ParameterIds.Num();
        for (int32 Index = 0; bParameterContract && Index < ParameterIds.Num(); ++Index)
            bParameterContract = Context.Scenario.Parameters[Index].ParameterId == ParameterIds[Index] &&
                Context.Scenario.Parameters[Index].DefaultValue.Type == (bSnapshots && Index < 3 ? EDivineBeastsDemoParameterType::Integer : EDivineBeastsDemoParameterType::Boolean);
        if (!bParameterContract) { Complete(Context.Token, FGamePlatformResult::Failure(TEXT("Demo.DebugParameterContract"), TEXT("诊断案例参数名称/类型与已编译合同不匹配。"))); return; }
        State->Provider = MakeShared<FDebugManualProvider>(State->Values);
        State->Provider->World = Context.World;
        const FString Suffix = Context.Token.RunId.ToString(EGuidFormats::Digits);
        State->Provider->Id = FName(*(TEXT("Manual.Debug.") + Suffix));
        State->CommandId = FName(*(TEXT("gp.Debug.Manual.") + Suffix));
        const uint64 BeforeSpawn = State->Generation;
        const auto Spawned = State->SpawnTarget();
        if (!Spawned.IsSuccess()) { Complete(Context.Token, Spawned); return; }
        if (State->Generation != BeforeSpawn) { Complete(Context.Token, FGamePlatformResult::Cancelled(TEXT("创建目标期间案例已停止，迟到目标已进入原清理链。"))); return; }
        State->bPrepared = true;
        State->Observe(TEXT("已创建本Run的弱目标。注册、采集与命令元数据均需显式操作；无Ticker或定时采集。"));
        Complete(Context.Token, State->bPrepared ? Spawned : FGamePlatformResult::Cancelled(TEXT("准备期间案例已停止或创建失败。")));
    }
    void Execute(FName Operation, const TArray<FDivineBeastsDemoParameterValue>& Input, FDivineBeastsDemoOperationCompletion Complete) override
    {
        const auto Token = State->Context.Token;
        if (!State->bPrepared || !State->Context.World.IsValid() || State->Context.World->bIsTearingDown || State->bCleaning)
        { Complete(Token, FGamePlatformResult::Failure(TEXT("Demo.DebugNotReady"), TEXT("诊断样板未准备或正在清理。"))); return; }
        const bool bSnapshots = State->Context.Scenario.FeatureId == TEXT("GamePlatformDebug.snapshots");
        if ((!bSnapshots && (Input.Num() != 1 || Input[0].Type != EDivineBeastsDemoParameterType::Boolean)) || (bSnapshots && (Input.Num() != 5 || Input[0].Type != EDivineBeastsDemoParameterType::Integer || Input[0].IntegerValue < 0 || Input[0].IntegerValue > 1000000 ||
            Input[1].Type != EDivineBeastsDemoParameterType::Integer || Input[1].IntegerValue < 1 || Input[1].IntegerValue > 256 ||
            Input[2].Type != EDivineBeastsDemoParameterType::Integer || Input[2].IntegerValue < 1 || Input[2].IntegerValue > 1024 ||
            Input[3].Type != EDivineBeastsDemoParameterType::Boolean || Input[4].Type != EDivineBeastsDemoParameterType::Boolean)))
        { Complete(Token, FGamePlatformResult::Failure(TEXT("Demo.DebugParameters"), TEXT("快照需要计数0..1000000、字段1..256、字符1..1024及两个布尔值；命令元数据只接收变更分类布尔值。非法值不夹紧。"))); return; }
        if (bSnapshots)
        {
            State->Values->Counter = Input[0].IntegerValue; State->Values->FieldCount = static_cast<int32>(Input[1].IntegerValue);
            State->Values->ValueLength = static_cast<int32>(Input[2].IntegerValue); State->Values->bExpensive = Input[4].bBooleanValue;
        }
        auto& Registry = FGamePlatformDebugRegistry::Get();
        auto Result = FGamePlatformResult::Success();
        FString Observation;
        const uint64 ExpectedGeneration = State->Generation;
        if (Operation == TEXT("RegisterProvider"))
        {
            if (State->bProviderOwned || !Registry.RegisterStateProvider(State->Provider.ToSharedRef()))
                Result = FGamePlatformResult::Failure(TEXT("Demo.DebugDuplicateProvider"), TEXT("本次提供者已注册或注册失败，保留原所有权。"));
            else State->bProviderOwned = true;
            Observation = TEXT("提供者注册归属：") + LexToString(State->bProviderOwned);
        }
        else if (Operation == TEXT("UnregisterProvider"))
        {
            if (!State->bProviderOwned) Result = FGamePlatformResult::Failure(TEXT("Demo.DebugProviderNotOwned"), TEXT("本例尚未持有提供者注册。"));
            else { Registry.UnregisterStateProvider(State->Provider->Id); State->bProviderOwned = false; }
            Observation = TEXT("本例提供者注册已撤销；未改变其他提供者。");
        }
        else if (Operation == TEXT("ListProviders"))
        {
            const auto Providers = Registry.GetProviderIds();
            Observation = FString::Printf(TEXT("原生提供者目录共%d项；本例%s；ID=%s；Enabled=%s"), Providers.Num(),
                Providers.Contains(State->Provider->Id) ? TEXT("已登记") : TEXT("未登记"), *State->Provider->Id.ToString(),
                Registry.IsProviderEnabled(State->Provider->Id) ? TEXT("是") : TEXT("否"));
        }
        else if (Operation == TEXT("Pause") || Operation == TEXT("Resume"))
        {
            if (!State->bProviderOwned || !Registry.SetProviderEnabled(State->Provider->Id, Operation == TEXT("Resume")))
                Result = FGamePlatformResult::Failure(TEXT("Demo.DebugEnable"), TEXT("本例提供者未注册，不能切换启用状态。"));
            Observation = TEXT("原生提供者Enabled：") + LexToString(Registry.IsProviderEnabled(State->Provider->Id));
        }
        else if (Operation == TEXT("Execute"))
        {
            FGamePlatformDebugCollectContext Context;
            Context.World = State->Context.World; Context.Target = State->Target; Context.RequestId = FGuid::NewGuid();
            Context.SourceView = State->Context.World->GetNetMode() == NM_Client ? EGamePlatformDebugSourceView::Client : EGamePlatformDebugSourceView::LocalAuthority;
            Context.bAllowExpensive = bSnapshots && Input[3].bBooleanValue;
            FGamePlatformDebugSnapshot Snapshot;
            const bool bCollected = Registry.CollectSnapshot(State->Provider->Id, Context, Snapshot);
            Observation = FString::Printf(TEXT("原生采集=%s；字段=%d；截断=%s；Target=%s；Revision=%llu\n"),
                bCollected ? TEXT("成功") : TEXT("失败"), Snapshot.Fields.Num(), Snapshot.bTruncated ? TEXT("是") : TEXT("否"),
                *Snapshot.TargetId.ToString(EGuidFormats::DigitsWithHyphens), static_cast<unsigned long long>(Snapshot.Revision));
            Observation += Registry.BuildSanitizedSummary(Snapshot);
            if (!bCollected) Result = FGamePlatformResult::Failure(TEXT("Demo.DebugCollectFailed"), Snapshot.FailureReason);
        }
        else if (Operation == TEXT("RefreshRevision"))
            Observation = TEXT("平台强制修订：") + LexToString(Registry.ForceRefreshRevision());
        else if (Operation == TEXT("RegisterCommand"))
        {
            FGamePlatformDebugCommandDescriptor Descriptor;
            Descriptor.Name = State->CommandId; Descriptor.Category = TEXT("ManualDiagnostics"); Descriptor.Help = TEXT("人工样板命令元数据；没有控制台执行者");
            Descriptor.Kind = !bSnapshots && Input[0].bBooleanValue ? EGamePlatformDebugCommandKind::Mutating : EGamePlatformDebugCommandKind::ReadOnly;
            Descriptor.RequiredPrivilege = TEXT("Developer"); Descriptor.AllowedBuilds = {TEXT("Development")};
            if (State->bCommandOwned || !Registry.RegisterCommandDescriptor(Descriptor))
                Result = FGamePlatformResult::Failure(TEXT("Demo.DebugDuplicateCommand"), TEXT("本例命令描述已存在或注册失败，保留原注册。"));
            else State->bCommandOwned = true;
            Observation = TEXT("本例命令描述注册归属：") + LexToString(State->bCommandOwned) + TEXT("；未执行任何控制台命令。");
        }
        else if (Operation == TEXT("UnregisterCommand"))
        {
            if (!State->bCommandOwned) Result = FGamePlatformResult::Failure(TEXT("Demo.DebugCommandNotOwned"), TEXT("本例尚未持有命令描述。"));
            else { Registry.UnregisterCommandDescriptor(State->CommandId); State->bCommandOwned = false; }
            Observation = TEXT("只撤销本例命令描述，不触及命令执行者。");
        }
        else if (Operation == TEXT("ListCommands"))
        {
            const auto Commands = Registry.GetCommandDescriptors();
            const auto* Own = Commands.FindByPredicate([this](const auto& Value) { return Value.Name == State->CommandId; });
            Observation = FString::Printf(TEXT("原生目录共%d条；本例=%s；Developer资格元数据仅为描述，不能据此执行命令。"), Commands.Num(), Own ? *Own->Name.ToString() : TEXT("未登记"));
            if (Own) Observation += FString::Printf(TEXT("\n分类=%s；权限=%s；类型=%s；构建=%s\n帮助=%s；参数描述=%s"),
                *Own->Category.ToString(), *Own->RequiredPrivilege.ToString(), Own->Kind == EGamePlatformDebugCommandKind::Mutating ? TEXT("可变元数据") : TEXT("只读元数据"),
                *FString::Join(Own->AllowedBuilds,TEXT(",")), *Own->Help, *Own->Args);
        }
        else if (Operation == TEXT("InvalidateTarget"))
        {
            if (AActor* Actor = State->Target.Get())
            {
                if (!Actor->Destroy()) Result = FGamePlatformResult::Failure(TEXT("Demo.DebugDestroy"), TEXT("自己的诊断目标未销毁。"));
                else { State->Target.Reset(); State->Provider->Target.Reset(); Registry.PruneExpiredTargets(); }
            }
            else Result = FGamePlatformResult::Failure(TEXT("Demo.DebugNoTarget"), TEXT("本例目标已失效。"));
            Observation = TEXT("目标已失效；后续采集必须明确拒绝，Reset可重建本例目标。");
        }
        else if (Operation == TEXT("Reset"))
        {
            Result = State->Clear();
            if (Result.IsSuccess() && State->bPrepared && State->Generation == ExpectedGeneration) Result = State->SpawnTarget();
            Observation = TEXT("重置只清理本例注册/目标；重新注册后再采集。");
        }
        else Result = FGamePlatformResult::Unsupported(TEXT("Demo.DebugOperation"), TEXT("未登记的诊断人工操作。"));
        if (Observation.IsEmpty()) Observation = Result.Message;
        State->Observe(Observation);
        if (!State->bPrepared || State->Generation != ExpectedGeneration)
            Result = FGamePlatformResult::Cancelled(TEXT("诊断通知期间案例已停止，旧命令不恢复显示。"));
        Complete(Token, Result);
    }
    void Stop(FDivineBeastsDemoOperationCompletion Complete) override
    {
        State->bPrepared = false; ++State->Generation;
        // 原生SpawnActor会同步通知外部观察者；目标尚未返回时不能提前宣称清理完成。
        if (State->bSpawningTarget)
        {
            if (State->DeferredStop) Complete(State->Context.Token, FGamePlatformResult::Failure(TEXT("Demo.DebugStopInFlight"), TEXT("原停止完成端仍在等待目标创建收敛。")));
            else State->DeferredStop = MoveTemp(Complete);
            return;
        }
        Complete(State->Context.Token, State->Clear());
    }
};
}
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateDiagnosticsDebugScenario() { return MakeUnique<FDebugManualScenario>(); }
}
