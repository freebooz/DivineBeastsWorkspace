// 神兽联盟项目环境群落的共用意图事务。Bird/Fauna仅映射目录与参数，统一Data预载/Presentation提交。
// 游戏线程；注册身份隔离Owner，同步通知后核对MutationToken，不保存跨通知TMap元素引用。
// 这里没有Niagara实例、逐个体Tick或资产加载器；真实执行/预算仍属于唯一GamePlatformVFX。
// 中文生命周期及审查证据见Docs/Implementation/BirdFlockAudit_20261011.md。
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "Interfaces/GamePlatformVFXService.h"
#include "Components/SplineComponent.h"

FGameplayTag UDivineBeastsPresentationClientSubsystem::GetEnvironmentFlockTag(
    const FEnvironmentFlockState& Group, FName SpeciesId)
{
    return Group.Kind == EEnvironmentFlockKind::Bird
        ? FDivineBeastsPresentationProjectCatalog::GetBirdFlockTag(SpeciesId)
        : FDivineBeastsPresentationProjectCatalog::GetFaunaTag(SpeciesId);
}

bool UDivineBeastsPresentationClientSubsystem::StartEnvironmentFlock(
    FEnvironmentFlockState& Group, FName Id, FName SpeciesId, int32 Count, int32 Seed,
    const FVector& WorldOrigin,
    UObject* Owner, const FGuid& RegistrationToken, FString& OutError,
    const FDivineBeastsBirdFlockFlightSettings* BirdFlight)
{
    check(IsInGameThread());
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> KeepService(this);
    OutError.Reset();
    if (Id.IsNone() || Count < 0 || Count > 512 || Seed < 0 || Seed > 1000000 || WorldOrigin.ContainsNaN())
    {
        OutError = TEXT("环境群落标识不能为空；数量须0..512，种子须0..1000000，世界坐标须为有限值。");
        return false;
    }
    UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (bClosing || bStoppingEnvironmentFlocks || Group.bStopping || !World || World->bIsTearingDown ||
        World->GetNetMode() == NM_DedicatedServer || !PlatformPresentation ||
        (Owner && Owner->GetWorld() != World))
    {
        OutError = TEXT("环境群落所属本地世界尚未就绪、正在停止或Owner不属于当前世界。");
        return false;
    }
    const TStrongObjectPtr<UWorld> KeepWorld(World);
    // 所有候选校验发生于原有合法群落被替换之前；Fauna继续走原始接口。
    if (BirdFlight && (Group.Kind != EEnvironmentFlockKind::Bird ||
        !BirdFlight->ValidateForWorld(World, OutError)))
    {
        if (OutError.IsEmpty()) OutError = TEXT("群落类型不支持鸟类路径配置。");
        return false;
    }
    // LocalPlayer跨图仍复用同一子系统；旧世界意图不能遮挡新世界同名请求。
    if ((!Group.Requests.IsEmpty() || Group.Pack.IsValid()) && Group.World.Get() != World)
    {
        StopAllEnvironmentFlocks(Group);
        if (bClosing || !GetLocalPlayer() || GetLocalPlayer()->GetWorld() != World || World->bIsTearingDown)
        {
            OutError = TEXT("清理旧环境群落期间世界已经改变。");
            return false;
        }
    }
    FEnvironmentFlockRequestState* Existing = Group.Requests.Find(Id);
    if (Existing && (Existing->RegistrationToken != RegistrationToken || Existing->Owner.Get() != Owner) &&
        (!Existing->RegistrationToken.IsValid() || Existing->Owner.IsValid()))
    {
        OutError = TEXT("环境群落标识已经由另一个活跃场景Owner或显式服务请求占用。");
        return false;
    }
    if (Count == 0)
    {
        if (RegistrationToken.IsValid()) StopEnvironmentFlockForOwner(Group, Id, RegistrationToken);
        else StopEnvironmentFlock(Group, Id);
        return true;
    }
    if (!GetEnvironmentFlockTag(Group, SpeciesId).IsValid())
    {
        OutError = Group.Kind == EEnvironmentFlockKind::Bird
            ? TEXT("鸟种无效：允许Seagull、Swallow、Eagle。")
            : TEXT("群落物种无效：允许ButterflyAzure、FishSilver。");
        return false;
    }
    if (!Existing && Group.Requests.Num() >= 16)
    {
        OutError = TEXT("每个本地玩家世界中，同类目录最多16个环境群落请求，防止无界场景请求。");
        return false;
    }

    if (Existing && Existing->SpeciesId == SpeciesId && Existing->Count == Count && Existing->Seed == Seed &&
        Existing->Origin.Equals(WorldOrigin) && Existing->RegistrationToken == RegistrationToken &&
        Existing->Owner.Get() == Owner && Existing->bExplicitBirdFlight == (BirdFlight != nullptr) &&
        (!BirdFlight || Existing->BirdFlight.EquivalentTo(*BirdFlight)))
    {
        const FGuid MutationToken = Existing->MutationToken;
        Existing->bAwaitingExplicitRetry = false; // 本ID的显式Apply重新武装一次，不启动循环。
        if (!EnsureEnvironmentFlockPack(Group, OutError)) return false;
        SubmitPendingEnvironmentFlocks(Group);
        const auto* Current = Group.Requests.Find(Id);
        if (Current && Current->MutationToken == MutationToken) return true;
        OutError = TEXT("环境群落请求在同步通知中已被更新或停止。");
        return false;
    }

    const bool bHadPrevious = Existing != nullptr;
    const FEnvironmentFlockRequestState Previous = Existing ? *Existing : FEnvironmentFlockRequestState();
    FEnvironmentFlockRequestState Desired;
    Desired.SpeciesId = SpeciesId; Desired.Count = Count; Desired.Seed = Seed; Desired.Origin = WorldOrigin;
    if (BirdFlight)
    {
        Desired.BirdFlight = *BirdFlight;
        Desired.bExplicitBirdFlight = true;
    }
    Desired.Owner = Owner; Desired.RegistrationToken = RegistrationToken;
    Desired.MutationToken = FGuid::NewGuid();
    const FGuid MutationToken = Desired.MutationToken;
    Group.World = World;
    // 先发布意图再调用会广播的资源/取消接口；回调只能修改当前条目，外层不能无条件写回旧快照。
    Group.Requests.Add(Id, Desired);
    if (!EnsureEnvironmentFlockPack(Group, OutError))
    {
        auto* Current = Group.Requests.Find(Id);
        if (Current && Current->MutationToken == MutationToken)
        {
            // 候选还未取消旧实例：同步拒绝时恢复合法旧意图和句柄。
            if (bHadPrevious) Group.Requests.Add(Id, Previous);
            else Group.Requests.Remove(Id);
        }
        else
        {
            // 更新已被重入操作替代，旧播放不能因为已从映射移出而失去清理所有权。
            CancelEnvironmentFlockRequest(Group, Previous.RequestId, Previous.SpeciesId, Id);
        }
        if (Group.Requests.IsEmpty()) StopAllEnvironmentFlocks(Group);
        return false;
    }
    // 数量改变需要重新Burst；此处取消仅作用于旧RequestId，不再跨回调保存TMap元素引用。
    CancelEnvironmentFlockRequest(Group, Previous.RequestId, Previous.SpeciesId, Id);
    const auto* Current = Group.Requests.Find(Id);
    if (bClosing || !Current || Current->MutationToken != MutationToken ||
        Group.World.Get() != World || World->bIsTearingDown)
    {
        OutError = TEXT("环境群落替换在取消通知中已被更新、停止或所属世界已退出。");
        return false;
    }
    SubmitPendingEnvironmentFlocks(Group);
    Current = Group.Requests.Find(Id);
    if (Current && Current->MutationToken == MutationToken) return true;
    OutError = TEXT("环境群落提交在同步通知中已被更新或停止。");
    return false;
}

bool UDivineBeastsPresentationClientSubsystem::EnsureEnvironmentFlockPack(FEnvironmentFlockState& Group, FString& OutError)
{
    // 上下文切换期间只接纳Desired；新Context提交后统一恢复，不启动旧上下文的目录。
    if (EnvironmentFlockContextSuspensions > 0) return true;
    if (bClosing || bStoppingEnvironmentFlocks || Group.bStopping || Group.Requests.IsEmpty()) return false;
    if (Group.Pack.IsValid())
    {
        const auto State = GetContentPackState(Group.Pack, OutError);
        if (State == EDivineBeastsPresentationContentPackState::Loading ||
            State == EDivineBeastsPresentationContentPackState::Active) return true;
        const auto Old = Group.Pack; Group.Pack = {};
        DeactivateContentPack(Old);
    }
    if (bClosing || bStoppingEnvironmentFlocks || Group.bStopping || Group.Requests.IsEmpty()) return false;
    // 注销通知可能已重入建立新代Pack；不能再次激活并覆盖后来发布的Pending身份。
    if (Group.Pack.IsValid())
    {
        const auto CurrentState = GetContentPackState(Group.Pack, OutError);
        if (CurrentState == EDivineBeastsPresentationContentPackState::Loading ||
            CurrentState == EDivineBeastsPresentationContentPackState::Active) return true;
    }
    FDivineBeastsPresentationContentPackFragment Fragment;
    Fragment.ContentPackId = Group.Kind == EEnvironmentFlockKind::Bird
        ? FName(TEXT("DBA.Environment.BirdFlock")) : FName(TEXT("DBA.Environment.Fauna"));
    Fragment.Revision = 1; Fragment.bRequiredPreload = true;
    Fragment.LifecycleScope = EGamePlatformPresentationContextScope::World;
    Fragment.CatalogFragment = Group.Kind == EEnvironmentFlockKind::Bird
        ? FDivineBeastsPresentationProjectCatalog::BuildBirdFlockFragment()
        : FDivineBeastsPresentationProjectCatalog::BuildFaunaFragment();
    Fragment.CatalogFragment.Scope = EGamePlatformPresentationCatalogScope::ContentPack;
    Fragment.CatalogFragment.OwnerScopeId = Fragment.ContentPackId;
    Fragment.CatalogFragment.LifecycleScope = Fragment.LifecycleScope;
    for (auto& Entry : Fragment.CatalogFragment.Entries) Entry.Scope = EGamePlatformPresentationCatalogScope::ContentPack;
    // Pending身份在预载诊断广播前写入成员；广播内StopAll能取消这次真实申请，不留下无主租约。
    const auto Accepted = ActivateContentPack(Fragment, OutError, &Group.Pack);
    if (Accepted.IsValid() && (Accepted.Id != Group.Pack.Id || Accepted.Generation != Group.Pack.Generation))
        DeactivateContentPack(Accepted);
    if (Group.Pack.IsValid())
    {
        FString StateError;
        const auto State = GetContentPackState(Group.Pack, StateError);
        if (State == EDivineBeastsPresentationContentPackState::Loading ||
            State == EDivineBeastsPresentationContentPackState::Active)
        {
            OutError.Reset();
            return true;
        }
        Group.Pack = {};
        if (OutError.IsEmpty()) OutError = StateError;
    }
    if (OutError.IsEmpty()) OutError = TEXT("环境群落内容包申请被取消或未能进入Loading/Active。");
    return false;
}

void UDivineBeastsPresentationClientSubsystem::SubmitPendingEnvironmentFlocks(FEnvironmentFlockState& Group)
{
    check(IsInGameThread());
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> KeepService(this);
    if (bClosing || bStoppingEnvironmentFlocks || Group.bStopping || EnvironmentFlockContextSuspensions > 0 ||
        !Group.Pack.IsValid() || !PlatformPresentation) return;
    FString Error;
    if (GetContentPackState(Group.Pack, Error) != EDivineBeastsPresentationContentPackState::Active) return;
    UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (!World || Group.World.Get() != World || World->bIsTearingDown || World->GetNetMode() == NM_DedicatedServer) return;
    const TStrongObjectPtr<UWorld> KeepWorld(World);
    if (!BindEnvironmentFlockCompletion(World)) return;
    TArray<FName> Keys; Group.Requests.GenerateKeyArray(Keys);
    for (FName Key : Keys)
    {
        if (bClosing || bStoppingEnvironmentFlocks || Group.bStopping || EnvironmentFlockContextSuspensions > 0 ||
            Group.World.Get() != World || World->bIsTearingDown) return;
        // 前一请求的同步通知可能已撤销/替换目录；每次提交仍必须拥有本代Active资源。
        if (GetContentPackState(Group.Pack, Error) != EDivineBeastsPresentationContentPackState::Active) return;
        auto* State = Group.Requests.Find(Key);
        if (!State) continue;
        if (State->RegistrationToken.IsValid() && (!State->Owner.IsValid() || State->Owner->GetWorld() != World))
        {
            const FGuid DeadRegistration = State->RegistrationToken;
            StopEnvironmentFlockForOwner(Group, Key, DeadRegistration);
            continue;
        }
        if (State->RequestId.IsValid() || State->Count <= 0 || State->bAwaitingExplicitRetry) continue;
        const auto Snapshot = *State;
        FGamePlatformPresentationRequest Request;
        Request.RequestId = FGuid::NewGuid();
        Request.SemanticTag = GetEnvironmentFlockTag(Group, Snapshot.SpeciesId);
        Request.ContextId = Key; Request.SourceLocation = Snapshot.Origin;
        Request.ImpactLocation = Snapshot.Origin; Request.ImpactNormal = FVector::UpVector;
        Request.Priority = EGamePlatformPresentationPriority::Low;
        Request.Lifetime = EGamePlatformPresentationLifetime::Persistent;
        Request.PredictionState = EGamePlatformPresentationPredictionState::Confirmed;
        if (Group.Kind == EEnvironmentFlockKind::Bird)
        {
            Request.IntegerParameters.Add(TEXT("User.BirdCount"), Snapshot.Count);
            if (Snapshot.bExplicitBirdFlight)
            {
                FString FlightError;
                if (!Snapshot.BirdFlight.ValidateForWorld(World, FlightError))
                {
                    State->bAwaitingExplicitRetry = true;
                    UE_LOG(LogTemp, Warning, TEXT("[BirdFlock] 提交前路径失效：%s"), *FlightError);
                    continue;
                }
                const auto& Flight = Snapshot.BirdFlight;
                Request.IntegerParameters.Add(TEXT("User.FlightMode"), static_cast<int32>(Flight.FlightMode));
                Request.IntegerParameters.Add(TEXT("User.FlockSeed"), Flight.Seed);
                if (Flight.bUseSpline)
                {
                    // 利用既有中立附着通道接入已注册世界Spline。
                    // Niagara DI的AttachParentOnly只读取当前附着父组件，不能保存跨世界Actor。
                    Request.AttachComponent = Flight.Spline.Get();
                }
                if (Flight.bEnableFlocking)
                {
                    Request.FloatParameters.Add(TEXT("User.SeparationWeight"), Flight.SeparationWeight);
                    Request.FloatParameters.Add(TEXT("User.AlignmentWeight"), Flight.AlignmentWeight);
                    Request.FloatParameters.Add(TEXT("User.CohesionWeight"), Flight.CohesionWeight);
                }
                if (Flight.GlideBlend > 0.f)
                {
                    Request.FloatParameters.Add(TEXT("User.GlideBlend"), Flight.GlideBlend);
                }
            }
        }
        else
        {
            Request.IntegerParameters.Add(TEXT("User.FaunaCount"), Snapshot.Count);
            Request.IntegerParameters.Add(TEXT("User.FaunaSeed"), Snapshot.Seed);
        }
        State->RequestId = Request.RequestId;
        const auto Result = SubmitProjectRequest(Request);
        if (Result != EGamePlatformPresentationSubmitResult::Submitted)
        {
            if (auto* Current = Group.Requests.Find(Key); Current && Current->RequestId == Request.RequestId)
                Current->RequestId.Invalidate();
            UE_LOG(LogTemp, Warning, TEXT("[EnvironmentFlock] 请求未提交：%s，状态%d"), *Key.ToString(), static_cast<int32>(Result));
        }
    }
}


bool UDivineBeastsPresentationClientSubsystem::BindEnvironmentFlockCompletion(UWorld* World)
{
    check(IsInGameThread());
    if (bClosing || bStoppingEnvironmentFlocks || !IsValid(World) || World->bIsTearingDown ||
        World->GetNetMode() == NM_DedicatedServer) return false;
    if (EnvironmentCompletionWorld.Get() == World && EnvironmentCompletionHandle.IsValid() &&
        EnvironmentCompletionToken.IsValid()) return true;
    UnbindEnvironmentFlockCompletion();
    auto* Service = IGamePlatformVFXService::Get(World);
    if (!Service) return false;
    const TWeakObjectPtr<UWorld> BoundWorld(World);
    const FGuid BindingToken = FGuid::NewGuid();
    EnvironmentCompletionWorld = BoundWorld;
    EnvironmentCompletionToken = BindingToken;
    // 在Submit之前注册，兼容平台同步失败与真实异步完成。Public头不依赖VFX回执类型。
    EnvironmentCompletionHandle = Service->AddCompletionHandler(
        FGamePlatformVFXPlaybackCompleted::FDelegate::CreateWeakLambda(this,
            [this, BoundWorld, BindingToken](const FGamePlatformVFXPlaybackSnapshot& Snapshot)
            {
                check(IsInGameThread());
                if (bClosing || bStoppingEnvironmentFlocks || !BoundWorld.IsValid() ||
                    EnvironmentCompletionWorld != BoundWorld || EnvironmentCompletionToken != BindingToken ||
                    !Snapshot.Handle.IsValid() || !Snapshot.Handle.BelongsToWorld(BoundWorld.Get()) ||
                    !Snapshot.RequestId.IsValid() || Snapshot.ParentHandle.IsValid()) return;
                // 复合子实例与父请求共享RequestId，只有无ParentHandle的真实顶层终态可清项目投影。
                switch (Snapshot.State)
                {
                case EGamePlatformVFXPlaybackState::Completed:
                case EGamePlatformVFXPlaybackState::Cancelled:
                case EGamePlatformVFXPlaybackState::Failed:
                case EGamePlatformVFXPlaybackState::ContentRevoked:
                case EGamePlatformVFXPlaybackState::WorldDestroyed:
                    break;
                default:
                    return;
                }
                const FGuid CompletedRequestId = Snapshot.RequestId;
                // 仅改当前字段；这里不调用任何外部服务/广播/Apply，容器遍历期间不允许再入。
                for (FEnvironmentFlockState* Group : {&BirdFlocks, &AmbientFauna})
                {
                    if (Group->World != BoundWorld) continue;
                    for (auto& Pair : Group->Requests)
                    {
                        // 每次Submit生成新RequestId；旧Mutation/旧播放的迟到终态不能命中新请求。
                        if (Pair.Value.RequestId != CompletedRequestId) continue;
                        Pair.Value.RequestId.Invalidate();
                        Pair.Value.bAwaitingExplicitRetry = true;
                    }
                }
            }));
    if (EnvironmentCompletionHandle.IsValid()) return true;
    EnvironmentCompletionWorld.Reset();
    EnvironmentCompletionToken.Invalidate();
    return false;
}

void UDivineBeastsPresentationClientSubsystem::UnbindEnvironmentFlockCompletion()
{
    check(IsInGameThread());
    const auto PreviousWorld = EnvironmentCompletionWorld;
    const FDelegateHandle PreviousHandle = EnvironmentCompletionHandle;
    // 先失效订阅代次，再移除委托；已经进入平台Broadcast列表的旧回调也只能提前返回。
    EnvironmentCompletionWorld.Reset();
    EnvironmentCompletionHandle.Reset();
    EnvironmentCompletionToken.Invalidate();
    if (PreviousHandle.IsValid())
        if (auto* Service = IGamePlatformVFXService::Get(PreviousWorld.Get()))
            Service->RemoveCompletionHandler(PreviousHandle);
}

void UDivineBeastsPresentationClientSubsystem::CancelEnvironmentFlockRequest(
    const FEnvironmentFlockState& Group, const FGuid& RequestId, FName SpeciesId, FName Id)
{
    if (!RequestId.IsValid() || !PlatformPresentation) return;
    FGamePlatformPresentationRequest Cancel;
    Cancel.RequestId = RequestId; Cancel.ContextId = Id;
    Cancel.SemanticTag = GetEnvironmentFlockTag(Group, SpeciesId);
    Cancel.Priority = EGamePlatformPresentationPriority::Low;
    Cancel.Lifetime = EGamePlatformPresentationLifetime::Persistent;
    Cancel.PredictionState = EGamePlatformPresentationPredictionState::Cancelled;
    SubmitProjectRequest(Cancel);
}

bool UDivineBeastsPresentationClientSubsystem::StopEnvironmentFlockForOwner(
    FEnvironmentFlockState& Group, FName Id, const FGuid& RegistrationToken)
{
    check(IsInGameThread());
    const auto* State = Group.Requests.Find(Id);
    if (!RegistrationToken.IsValid() || !State || State->RegistrationToken != RegistrationToken) return false;
    StopEnvironmentFlock(Group, Id);
    return true;
}

void UDivineBeastsPresentationClientSubsystem::StopEnvironmentFlock(FEnvironmentFlockState& Group, FName Id)
{
    check(IsInGameThread());
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> KeepService(this);
    FEnvironmentFlockRequestState Removed;
    if (Group.Requests.RemoveAndCopyValue(Id, Removed))
        CancelEnvironmentFlockRequest(Group, Removed.RequestId, Removed.SpeciesId, Id);
    if (Group.Requests.IsEmpty() && Group.Pack.IsValid())
    {
        const auto Old = Group.Pack; Group.Pack = {};
        DeactivateContentPack(Old);
    }
}

void UDivineBeastsPresentationClientSubsystem::StopAllEnvironmentFlocks(FEnvironmentFlockState& Group)
{
    check(IsInGameThread());
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> KeepService(this);
    // 清理期间拒绝新Start；先撤销所有意图和Pack身份，再触发同步取消/注销通知。
    TGuardValue<bool> StopGuard(Group.bStopping, true);
    const auto Pending = MoveTemp(Group.Requests);
    Group.Requests.Reset();
    const auto Old = Group.Pack; Group.Pack = {};
    Group.World.Reset();
    for (const auto& Pair : Pending) CancelEnvironmentFlockRequest(Group, Pair.Value.RequestId, Pair.Value.SpeciesId, Pair.Key);
    if (Old.IsValid()) DeactivateContentPack(Old);
}

void UDivineBeastsPresentationClientSubsystem::SuspendEnvironmentFlocksForContextChange()
{
    check(IsInGameThread());
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> KeepService(this);
    // 先给两类群落同时设门闩，再允许任何取消通知；跨类型重入也只能排队Desired。
    if (++EnvironmentFlockContextSuspensions > 1) return;
    for (FEnvironmentFlockState* Group : {&BirdFlocks, &AmbientFauna})
    {
        UWorld* LocalWorld = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
        if ((!Group->Requests.IsEmpty() || Group->Pack.IsValid()) && Group->World.Get() != LocalWorld)
        {
            StopAllEnvironmentFlocks(*Group);
            continue;
        }
        const auto OldPack = Group->Pack; Group->Pack = {};
        TArray<TPair<FName, FEnvironmentFlockRequestState>> Playing;
        for (auto& Pair : Group->Requests)
        {
            if (Pair.Value.RequestId.IsValid()) Playing.Emplace(Pair.Key, Pair.Value);
            Pair.Value.RequestId.Invalidate();
            Pair.Value.bAwaitingExplicitRetry = false; // 新Context的资源代次可按存活Desired恢复一次。
        }
        // 不复制Desired再恢复：广播内Stop删除当前意图，Resume不能将其复活。
        for (const auto& Pair : Playing)
            CancelEnvironmentFlockRequest(*Group, Pair.Value.RequestId, Pair.Value.SpeciesId, Pair.Key);
        if (OldPack.IsValid()) DeactivateContentPack(OldPack);
    }
}

void UDivineBeastsPresentationClientSubsystem::ResumeEnvironmentFlocksAfterContextChange()
{
    check(IsInGameThread());
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> KeepService(this);
    if (EnvironmentFlockContextSuspensions <= 0 || --EnvironmentFlockContextSuspensions > 0 ||
        bClosing || bStoppingEnvironmentFlocks) return;
    for (FEnvironmentFlockState* Group : {&BirdFlocks, &AmbientFauna})
    {
        if (EnvironmentFlockContextSuspensions > 0 || bClosing || bStoppingEnvironmentFlocks) return;
        if (Group->bStopping || Group->Requests.IsEmpty()) continue;
        UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
        if (!World || Group->World.Get() != World || World->bIsTearingDown)
        {
            StopAllEnvironmentFlocks(*Group);
            continue;
        }
        FString Error;
        if (EnsureEnvironmentFlockPack(*Group, Error)) SubmitPendingEnvironmentFlocks(*Group);
        else UE_LOG(LogTemp, Warning, TEXT("[EnvironmentFlock] 新上下文资源尚未受理：%s"), *Error);
    }
}
