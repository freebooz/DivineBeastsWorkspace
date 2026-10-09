// 平台双端交互目标：服务器拥有当前实例/代次、选项和会话占位；Custom处理器拥有业务副作用，配置/退出同步取消占位。
// 平台交互目标状态组件：游戏线程由服务器Owner权威维护实例/代次/选项，生命周期内发布只读复制事实。
#include "Components/GamePlatformInteractableComponent.h"

#include "Components/GamePlatformInteractorComponent.h"
#include "Interfaces/GamePlatformInteractable.h"
// Owner权威、位置与IsValid继承转换均需要完整Actor类型，不依靠Unity/PCH间接包含。
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Settings/GamePlatformInteractionSettings.h"
#include "Types/GamePlatformInteractionSession.h"

namespace
{
/** 正向递增复制计数器；达到 int32 上限后回绕到 1，避免有符号整数溢出。 */
int32 AdvancePositiveCounter(int32 CurrentValue)
{
    return CurrentValue >= MAX_int32 ? 1 : CurrentValue + 1;
}
}

UGamePlatformInteractableComponent::UGamePlatformInteractableComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UGamePlatformInteractableComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* OwnerActor = GetOwner();
    if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
    {
        return;
    }

    if (!TargetInstanceId.IsValid())
    {
        TargetInstanceId = FGuid::NewGuid();
        TargetGeneration = FMath::Max(1, TargetGeneration);
        TargetRevision = FMath::Max(1, TargetRevision);
    }

    // 编辑器初始配置同样必须满足运行期约束；非法配置在服务器侧直接关闭交互，
    // 防止客户端展示后再被服务器拒绝形成“可见但永远不可用”的假候选。
    if (!ValidateOptions(Options))
    {
        bEnabled = false;
    }
}

void UGamePlatformInteractableComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(GetOwner()) && GetOwner()->HasAuthority())
    {
        TArray<TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>> Sessions;
        Sessions.Reserve(ActiveSessions.Num());
        for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : ActiveSessions)
        {
            Sessions.Add(Pair);
        }

        ActiveSessions.Reset();
        OccupancyCount = 0;

        for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : Sessions)
        {
            if (UGamePlatformInteractorComponent* Interactor = Pair.Value.Get())
            {
                Interactor->CancelFromTarget(
                    Pair.Key,
                    EGamePlatformInteractionCancelReason::TargetDestroyed);
            }
        }

        // EndPlay（结束生命周期）同样必须使用安全正整数计数器，避免极端长生命周期下 int32 溢出。
        TargetGeneration = AdvancePositiveCounter(TargetGeneration);
    }

    Super::EndPlay(EndPlayReason);
}

void UGamePlatformInteractableComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UGamePlatformInteractableComponent, TargetInstanceId);
    DOREPLIFETIME(UGamePlatformInteractableComponent, TargetGeneration);
    DOREPLIFETIME(UGamePlatformInteractableComponent, TargetRevision);
    DOREPLIFETIME(UGamePlatformInteractableComponent, Options);
    DOREPLIFETIME(UGamePlatformInteractableComponent, bEnabled);
    DOREPLIFETIME(UGamePlatformInteractableComponent, bConsumed);
    DOREPLIFETIME(UGamePlatformInteractableComponent, bToggleState);
    DOREPLIFETIME(UGamePlatformInteractableComponent, RemainingCharges);
    DOREPLIFETIME(UGamePlatformInteractableComponent, OccupancyCount);
}

const FGamePlatformInteractionOption*
UGamePlatformInteractableComponent::FindOption(FName OptionId) const
{
    return Options.FindByPredicate(
        [OptionId](const FGamePlatformInteractionOption& Option)
        {
            return Option.OptionId == OptionId;
        });
}

bool UGamePlatformInteractableComponent::IsOptionAvailable(
    const FGamePlatformInteractionOption& Option) const
{
    if (!IsInteractionEnabled() ||
        !Option.bEnabled ||
        !Option.IsStructurallyValid())
    {
        return false;
    }

    if (Option.CommitKind == EGamePlatformInteractionCommitKind::Harvest &&
        RemainingCharges <= 0)
    {
        return false;
    }

    if (Option.ConcurrencyPolicy ==
        EGamePlatformInteractionConcurrencyPolicy::Exclusive)
    {
        return OccupancyCount == 0;
    }

    return OccupancyCount < FMath::Max(1, Option.MaxConcurrent);
}

bool UGamePlatformInteractableComponent::CanContinueSession(
    const FGuid& SessionId,
    const FGamePlatformInteractionOption& Option) const
{
    const TWeakObjectPtr<UGamePlatformInteractorComponent>* ActiveInteractor =
        ActiveSessions.Find(SessionId);

    if (!ActiveInteractor ||
        !ActiveInteractor->IsValid() ||
        !IsInteractionEnabled() ||
        !Option.bEnabled ||
        !Option.IsStructurallyValid())
    {
        return false;
    }

    if (Option.CommitKind == EGamePlatformInteractionCommitKind::Harvest &&
        RemainingCharges <= 0)
    {
        return false;
    }

    return true;
}
bool UGamePlatformInteractableComponent::SetOptions(
    const TArray<FGamePlatformInteractionOption>& InOptions)
{
    if (!IsValid(GetOwner()) ||
        !GetOwner()->HasAuthority() ||
        !ValidateOptions(InOptions))
    {
        return false;
    }

    Options = InOptions;
    BumpRevision();
    CancelActiveSessions(
        EGamePlatformInteractionCancelReason::TargetRevisionChanged);
    return true;
}

bool UGamePlatformInteractableComponent::SetInteractionEnabled(bool bInEnabled)
{
    if (!IsValid(GetOwner()) ||
        !GetOwner()->HasAuthority() ||
        bEnabled == bInEnabled ||
        (bConsumed && bInEnabled))
    {
        return false;
    }

    bEnabled = bInEnabled;
    BumpRevision();
    if (!bEnabled)
    {
        CancelActiveSessions(
            EGamePlatformInteractionCancelReason::TargetUnavailable);
    }
    return true;
}

bool UGamePlatformInteractableComponent::SetRemainingCharges(
    int32 InRemainingCharges)
{
    if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
    {
        return false;
    }

    const int32 NewValue = FMath::Max(0, InRemainingCharges);
    if (RemainingCharges == NewValue)
    {
        return false;
    }

    RemainingCharges = NewValue;
    if (RemainingCharges == 0)
    {
        bEnabled = false;
    }

    BumpRevision();
    CancelActiveSessions(
        RemainingCharges <= 0
            ? EGamePlatformInteractionCancelReason::TargetUnavailable
            : EGamePlatformInteractionCancelReason::TargetRevisionChanged);
    return true;
}

void UGamePlatformInteractableComponent::AdvanceTargetGeneration()
{
    if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
    {
        return;
    }

    TArray<TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>> Sessions;
    Sessions.Reserve(ActiveSessions.Num());
    for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : ActiveSessions)
    {
        Sessions.Add(Pair);
    }

    TargetGeneration = AdvancePositiveCounter(TargetGeneration);
    ActiveSessions.Reset();
    OccupancyCount = 0;
    CommittedResults.Reset();
    CommittedSessionOrder.Reset();
    BumpRevision();

    for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : Sessions)
    {
        if (UGamePlatformInteractorComponent* Interactor = Pair.Value.Get())
        {
            Interactor->CancelFromTarget(
                Pair.Key,
                EGamePlatformInteractionCancelReason::TargetRevisionChanged);
        }
    }
}

bool UGamePlatformInteractableComponent::TryAcquireSession(
    const FGuid& SessionId,
    UGamePlatformInteractorComponent* Interactor,
    const FGamePlatformInteractionOption& Option,
    EGamePlatformInteractionError& OutError)
{
    OutError = EGamePlatformInteractionError::None;

    if (!IsValid(GetOwner()) ||
        !GetOwner()->HasAuthority() ||
        !SessionId.IsValid() ||
        !IsValid(Interactor))
    {
        OutError = EGamePlatformInteractionError::InvalidInteractor;
        return false;
    }

    // 异常销毁的 Interactor（交互发起器）不能永久占用 Shared/Exclusive（共享/独占）名额。
    PruneInvalidActiveSessions();

    if (!IsOptionAvailable(Option))
    {
        OutError =
            IsInteractionEnabled()
                ? EGamePlatformInteractionError::TargetBusy
                : EGamePlatformInteractionError::TargetUnavailable;
        return false;
    }

    if (Option.ConcurrencyPolicy ==
        EGamePlatformInteractionConcurrencyPolicy::Exclusive)
    {
        if (OccupancyCount > 0)
        {
            OutError = EGamePlatformInteractionError::TargetBusy;
            return false;
        }
    }
    else if (OccupancyCount >= FMath::Max(1, Option.MaxConcurrent))
    {
        OutError = EGamePlatformInteractionError::ConcurrentLimitReached;
        return false;
    }

    ActiveSessions.Add(SessionId, Interactor);
    OccupancyCount = ActiveSessions.Num();
    return true;
}

void UGamePlatformInteractableComponent::ReleaseSession(const FGuid& SessionId)
{
    if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
    {
        return;
    }

    ActiveSessions.Remove(SessionId);
    OccupancyCount = ActiveSessions.Num();
}

bool UGamePlatformInteractableComponent::CommitSession(
    const FGamePlatformInteractionSession& InSession,
    const FGamePlatformInteractionOption& InOption,
    FGamePlatformInteractionResult& OutResult)
{
    // 公开C++调用同样不得借用可被Custom回调修改的数组元素或会话对象。
    const FGamePlatformInteractionSession Session = InSession;
    const FGamePlatformInteractionOption Option = InOption;
    OutResult.RequestId = Session.RequestId;
    OutResult.SessionId = Session.SessionId;
    OutResult.OptionId = Option.OptionId;

    if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
    {
        OutResult.Error = EGamePlatformInteractionError::InvalidTarget;
        return false;
    }

    // 同一 Session（会话）的重复 Commit（提交）直接返回首次成功结果，
    // 不重复执行 Toggle/Consume/Harvest/Custom（切换/消费/采集/自定义）副作用。
    if (const FGamePlatformInteractionResult* CachedResult =
        CommittedResults.Find(Session.SessionId))
    {
        OutResult = *CachedResult;
        return OutResult.IsSuccess();
    }

    // TargetRevisionAtStart（目标起始修订号）只用于 Begin（开始）阶段的乐观并发校验。
    // 会话建立后，配置/可用性变更会通过 CancelActiveSessions（取消活动会话）显式失效；
    // 因此这里不能因另一个 Shared（共享）会话成功提交并推进 Revision（修订号）而误杀当前会话。
    if (Session.TargetGeneration != TargetGeneration ||
        !CanContinueSession(Session.SessionId, Option))
    {
        OutResult.Error =
            Session.TargetGeneration != TargetGeneration
                ? EGamePlatformInteractionError::StaleTargetGeneration
                : EGamePlatformInteractionError::TargetUnavailable;
        return false;
    }

    bool bCommitted = false;
    switch (Option.CommitKind)
    {
    case EGamePlatformInteractionCommitKind::Toggle:
        bToggleState = !bToggleState;
        bCommitted = true;
        break;

    case EGamePlatformInteractionCommitKind::Consume:
        if (!bConsumed)
        {
            bConsumed = true;
            bEnabled = false;
            bCommitted = true;
        }
        break;

    case EGamePlatformInteractionCommitKind::Harvest:
        if (RemainingCharges > 0)
        {
            --RemainingCharges;
            if (RemainingCharges == 0)
            {
                bEnabled = false;
            }
            bCommitted = true;
        }
        break;

    case EGamePlatformInteractionCommitKind::External:
        // External（外部结果）提交只确认本次 Interaction（交互）事实成立。
        // 目标最终 Consumed（已消费）状态必须由外部权威结果回调后显式 Finalize（最终确认）。
        bCommitted = true;
        break;

    case EGamePlatformInteractionCommitKind::Custom:
    default:
        if (IGamePlatformInteractable* Interface =
            Cast<IGamePlatformInteractable>(GetOwner()))
        {
            bCommitted = Interface->CommitInteraction(Session, Option);
            // 处理器副作用是否成功不等于原占位仍然有效；配置撤销/目标销毁后不缓存成功、不推进提交修订号。
            if (!IsValid(this) || !IsValid(GetOwner()) || Session.TargetGeneration != TargetGeneration ||
                !CanContinueSession(Session.SessionId, Option))
            { OutResult.Error = EGamePlatformInteractionError::TargetUnavailable; return false; }
        }
        else
        {
            OutResult.Error =
                EGamePlatformInteractionError::OutcomeHandlerUnavailable;
            return false;
        }
        break;
    }

    if (!bCommitted)
    {
        OutResult.Error = EGamePlatformInteractionError::OutcomeUnknown;
        return false;
    }

    BumpRevision();

    OutResult.Error = EGamePlatformInteractionError::None;
    OutResult.State = EGamePlatformInteractionSessionState::Completed;
    OutResult.FinalTargetRevision = TargetRevision;

    CommittedResults.Add(Session.SessionId, OutResult);
    CommittedSessionOrder.Add(Session.SessionId);

    const int32 MaxRememberedSessions =
        FMath::Max(
            8,
            GetDefault<UGamePlatformInteractionSettings>()
                ->MaxRecentRequests);

    while (CommittedSessionOrder.Num() > MaxRememberedSessions)
    {
        const FGuid Oldest = CommittedSessionOrder[0];
        CommittedSessionOrder.RemoveAt(
            0,
            1,
            EAllowShrinking::No);
        CommittedResults.Remove(Oldest);
    }

    return true;
}

bool UGamePlatformInteractableComponent::BeginExternalOutcomeReservation(
    const FGuid& ReservationId)
{
    if (!IsValid(GetOwner()) ||
        !GetOwner()->HasAuthority() ||
        !ReservationId.IsValid() ||
        bConsumed)
    {
        return false;
    }

    if (ExternalOutcomeReservationId.IsValid())
    {
        return ExternalOutcomeReservationId == ReservationId;
    }

    if (!bEnabled)
    {
        return false;
    }

    ExternalOutcomeReservationId = ReservationId;
    bEnabled = false;
    BumpRevision();
    CancelActiveSessions(
        EGamePlatformInteractionCancelReason::TargetUnavailable);
    return true;
}

bool UGamePlatformInteractableComponent::FinalizeExternalConsume(
    const FGuid& ReservationId)
{
    if (!IsValid(GetOwner()) ||
        !GetOwner()->HasAuthority() ||
        !ReservationId.IsValid() ||
        ExternalOutcomeReservationId != ReservationId ||
        bConsumed)
    {
        return false;
    }

    ExternalOutcomeReservationId.Invalidate();
    bConsumed = true;
    bEnabled = false;
    BumpRevision();
    return true;
}

bool UGamePlatformInteractableComponent::CancelExternalOutcomeReservation(
    const FGuid& ReservationId)
{
    if (!IsValid(GetOwner()) ||
        !GetOwner()->HasAuthority() ||
        !ReservationId.IsValid() ||
        ExternalOutcomeReservationId != ReservationId ||
        bConsumed)
    {
        return false;
    }

    ExternalOutcomeReservationId.Invalidate();
    bEnabled = true;
    BumpRevision();
    return true;
}

void UGamePlatformInteractableComponent::OnRep_State()
{
    OnStateChanged.Broadcast();
}

void UGamePlatformInteractableComponent::PruneInvalidActiveSessions()
{
    if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
    {
        return;
    }

    const int32 PreviousOccupancy = ActiveSessions.Num();
    for (auto It = ActiveSessions.CreateIterator(); It; ++It)
    {
        if (!It.Value().IsValid())
        {
            It.RemoveCurrent();
        }
    }

    OccupancyCount = ActiveSessions.Num();
    if (OccupancyCount != PreviousOccupancy)
    {
        OnStateChanged.Broadcast();
    }
}

void UGamePlatformInteractableComponent::CancelActiveSessions(
    EGamePlatformInteractionCancelReason Reason)
{
    if (!IsValid(GetOwner()) ||
        !GetOwner()->HasAuthority() ||
        ActiveSessions.IsEmpty())
    {
        return;
    }

    TArray<TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>> Sessions;
    Sessions.Reserve(ActiveSessions.Num());

    for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : ActiveSessions)
    {
        Sessions.Add(Pair);
    }

    for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : Sessions)
    {
        if (UGamePlatformInteractorComponent* Interactor = Pair.Value.Get())
        {
            Interactor->CancelFromTarget(Pair.Key, Reason);
        }
    }

    ActiveSessions.Reset();
    OccupancyCount = 0;
}

void UGamePlatformInteractableComponent::BumpRevision()
{
    TargetRevision = AdvancePositiveCounter(TargetRevision);
    OnStateChanged.Broadcast();
}

bool UGamePlatformInteractableComponent::ValidateOptions(
    const TArray<FGamePlatformInteractionOption>& InOptions) const
{
    const UGamePlatformInteractionSettings* Settings =
        GetDefault<UGamePlatformInteractionSettings>();

    TSet<FName> OptionIds;
    for (const FGamePlatformInteractionOption& Option : InOptions)
    {
        if (!Option.IsStructurallyValid() ||
            Option.MaxDistance > Settings->MaxConfiguredInteractionDistance ||
            (Option.Mode == EGamePlatformInteractionMode::Hold &&
             Option.HoldDuration > Settings->MaxHoldDuration) ||
            OptionIds.Contains(Option.OptionId))
        {
            return false;
        }

        OptionIds.Add(Option.OptionId);
    }

    return true;
}
