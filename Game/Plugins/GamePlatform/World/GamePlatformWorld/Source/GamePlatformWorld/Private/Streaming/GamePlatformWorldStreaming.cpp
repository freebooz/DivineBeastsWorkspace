#include "Streaming/GamePlatformWorldStreaming.h"

#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace
{
    bool IsRuntimeWorld(const UWorld* World)
    {
        return IsValid(World) && !World->bIsTearingDown && !IsRunningCommandlet()
            && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
    }

    bool IsOwnerInWorld(const TWeakObjectPtr<UObject>& Owner, const UWorld* World)
    {
        const UObject* Object = Owner.Get();
        return Object && World && !Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
            && (Object == World || Object->GetWorld() == World || Object->GetTypedOuter<UWorld>() == World);
    }

    // 稳定地址由具体类型TUniquePtr拥有；引擎接口没有虚析构，不能通过接口指针删除。
    struct FRequestSource final : IWorldPartitionStreamingSourceProvider
    {
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<UObject> Owner;
        FWorldPartitionStreamingSource Source;
        bool bEnabled = true;

        bool GetStreamingSource(FWorldPartitionStreamingSource& OutSource) const override
        {
            check(IsInGameThread());
            if (!bEnabled || !IsRuntimeWorld(World.Get()) || !IsOwnerInWorld(Owner, World.Get()))
            {
                return false;
            }
            OutSource = Source;
            return true;
        }

        const UObject* GetStreamingSourceOwner() const override { return Owner.Get(); }
    };
}

struct FGamePlatformWorldStreaming::FImpl
{
    struct FEntry
    {
        FGamePlatformWorldStreamingRequest Request;
        FGamePlatformWorldStreamingResult Result;
        TWeakObjectPtr<ULevelStreaming> Level;
        TWeakObjectPtr<UWorldPartitionSubsystem> PartitionSubsystem;
        TWeakObjectPtr<UWorldPartition> Partition;
        TUniquePtr<FRequestSource> Source;
        double PendingSinceSeconds = 0.0;

        void ReleaseDemand()
        {
            if (Source)
            {
                Source->bEnabled = false;
                if (UWorldPartitionSubsystem* Subsystem = PartitionSubsystem.Get())
                {
                    // 只移除自己的Provider；不清除其他来源，也不直接卸载任意cell。
                    Subsystem->UnregisterStreamingSourceProvider(Source.Get());
                }
                Source.Reset();
            }
            // Level标志是全局共享状态，没有可恢复的独占租约；绝不写回旧值。
            Level.Reset();
            Partition.Reset();
            PartitionSubsystem.Reset();
        }

        void Fail(FName Error)
        {
            Result = { EGamePlatformWorldStreamingState::Failed, Error };
            ReleaseDemand();
        }
    };

    TWeakObjectPtr<UWorld> World;
    FGuid Generation;
    TMap<FGuid, TUniquePtr<FEntry>> Entries;
    FDelegateHandle TearDownHandle;
    bool bClosed = false;

    FImpl(UWorld& InWorld, FGuid InGeneration) : World(&InWorld), Generation(InGeneration)
    {
        // 没有BeginPlay的世界不一定收到OnWorldEndPlay，必须在更早的TearDown撤销原生指针。
        TearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddLambda([this](UWorld* ClosingWorld)
        {
            if (World.Get() == ClosingWorld) { Shutdown(); }
        });
    }

    bool IsOpen() const { return !bClosed && Generation.IsValid() && IsRuntimeWorld(World.Get()); }

    FEntry* Find(const FGamePlatformWorldStreamingHandle& Handle) const
    {
        if (!Handle.IsValid() || Handle.ContextGeneration != Generation) { return nullptr; }
        const TUniquePtr<FEntry>* Entry = Entries.Find(Handle.RequestId);
        return Entry ? Entry->Get() : nullptr;
    }

    void Shutdown()
    {
        check(IsInGameThread());
        if (bClosed) { return; }
        bClosed = true;
        FWorldDelegates::OnWorldBeginTearDown.Remove(TearDownHandle);
        TearDownHandle.Reset();
        for (auto& Pair : Entries)
        {
            FEntry& Entry = *Pair.Value;
            Entry.ReleaseDemand();
            // 既有失败保留诊断；仅活跃请求转取消。
            if (Entry.Result.State == EGamePlatformWorldStreamingState::Pending
                || Entry.Result.State == EGamePlatformWorldStreamingState::Ready)
            {
                Entry.Result = { EGamePlatformWorldStreamingState::Cancelled, TEXT("WorldStreamingClosed") };
            }
        }
    }

    // false+None表示尚未完成；明确错误表示底层已失效，不能靠继续等待隐藏问题。
    bool Sample(FEntry& Entry, FName& OutError) const
    {
        OutError = NAME_None;
        UWorld* CurrentWorld = World.Get();
        if (!IsOwnerInWorld(Entry.Request.Owner, CurrentWorld))
        {
            OutError = TEXT("WorldStreamingOwnerInvalid");
            return false;
        }
        if (CurrentWorld->GetShouldForceUnloadStreamingLevels())
        {
            OutError = TEXT("WorldStreamingExternalChange");
            return false;
        }
        if (Entry.Source)
        {
            UWorldPartitionSubsystem* Subsystem = Entry.PartitionSubsystem.Get();
            UWorldPartition* Partition = Entry.Partition.Get();
            if (!Subsystem || !Partition || CurrentWorld->GetWorldPartition() != Partition
                || !Subsystem->IsStreamingSourceProviderRegistered(Entry.Source.Get()))
            {
                OutError = TEXT("WorldStreamingSourceLost");
                return false;
            }
            if (!Partition->IsStreamingEnabled()
                || (Partition->IsServer() && !Partition->IsServerStreamingEnabled()))
            {
                OutError = TEXT("WorldStreamingUnsupportedPartition");
                return false;
            }
            bool bMainPartitionRegistered = false;
            Subsystem->ForEachWorldPartition([&](UWorldPartition* Candidate)
            {
                bMainPartitionRegistered |= Candidate == Partition;
                return true;
            });
            if (!bMainPartitionRegistered || !Partition->IsInitialized() || !Partition->CanStream())
            {
                return false;
            }
            // 空策略/空分区/Provider未供源时官方完成查询可返回true。
            // 先确认引擎运行策略确实消费了本请求，不把注册本身视为完成。
            const bool bConsumed = Partition->GetStreamingSources().ContainsByPredicate(
                [&](const FWorldPartitionStreamingSource& Source) { return Source.Name == Entry.Source->Source.Name; });
            if (!bConsumed) { return false; }
            FWorldPartitionStreamingSource CurrentSource;
            if (!Entry.Source->GetStreamingSource(CurrentSource))
            {
                OutError = TEXT("WorldStreamingOwnerInvalid");
                return false;
            }
            return Subsystem->IsStreamingCompleted(Entry.Source.Get());
        }

        ULevelStreaming* Level = Entry.Level.Get();
        if (!Level || Level->GetWorld() != CurrentWorld || !CurrentWorld->GetStreamingLevels().Contains(Level))
        {
            OutError = TEXT("WorldStreamingLevelRemoved");
            return false;
        }
        if (FName(*UWorld::RemovePIEPrefix(Level->GetWorldAssetPackageName()))
                != FName(*UWorld::RemovePIEPrefix(Entry.Request.LevelPackage.ToString()))
            || Level->GetIsRequestingUnloadAndRemoval() || !Level->ShouldBeLoaded()
            || (Entry.Request.RequestedState == EGamePlatformWorldStreamingRequestedState::Activated && !Level->ShouldBeVisible()))
        {
            OutError = TEXT("WorldStreamingExternalChange");
            return false;
        }
        const ELevelStreamingState State = Level->GetLevelStreamingState();
        if (State == ELevelStreamingState::FailedToLoad)
        {
            OutError = TEXT("WorldStreamingLoadFailed");
            return false;
        }
        if (State == ELevelStreamingState::Removed)
        {
            OutError = TEXT("WorldStreamingLevelRemoved");
            return false;
        }
        if (Entry.Request.RequestedState == EGamePlatformWorldStreamingRequestedState::Activated)
        {
            return State == ELevelStreamingState::LoadedVisible && Level->IsLevelLoaded() && Level->IsLevelVisible();
        }
        return Level->IsLevelLoaded()
            && (State == ELevelStreamingState::LoadedNotVisible || State == ELevelStreamingState::LoadedVisible);
    }
};

FGamePlatformWorldStreaming::FGamePlatformWorldStreaming(UWorld& World, FGuid ContextGeneration)
{
    check(IsInGameThread());
    Impl = MakeUnique<FImpl>(World, ContextGeneration);
}

FGamePlatformWorldStreaming::~FGamePlatformWorldStreaming() { Shutdown(); }

FGamePlatformWorldStreamingHandle FGamePlatformWorldStreaming::Request(
    const FGamePlatformWorldStreamingRequest& Request, FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    auto Reject = [&](FName Code, const TCHAR* Message)
    {
        OutResult = FGamePlatformResult::Failure(Code, Message);
        return FGamePlatformWorldStreamingHandle{};
    };
    if (Impl->bClosed) { return Reject(TEXT("WorldStreamingClosed"), TEXT("世界流送协调器已关闭。")); }
    if (!Impl->IsOpen()) { return Reject(TEXT("WorldStreamingWorldInvalid"), TEXT("需要有效的Game/PIE世界及非空代次，禁止Commandlet流送。")); }
    if (Request.ContextGeneration != Impl->Generation)
    {
        return Reject(TEXT("WorldStreamingGenerationMismatch"), TEXT("请求世界代次不匹配。"));
    }
    UWorld* World = Impl->World.Get();
    if (!IsOwnerInWorld(Request.Owner, World))
    {
        return Reject(TEXT("WorldStreamingOwnerInvalid"), TEXT("请求拥有者失效或不属于当前世界。"));
    }
    if (!FMath::IsFinite(Request.TargetLocation.X) || !FMath::IsFinite(Request.TargetLocation.Y)
        || !FMath::IsFinite(Request.TargetLocation.Z) || !FMath::IsFinite(Request.TimeoutSeconds)
        || Request.TimeoutSeconds <= 0.0 || Request.Priority < 0 || Request.Priority > 255
        || (Request.RequestedState != EGamePlatformWorldStreamingRequestedState::Loaded
            && Request.RequestedState != EGamePlatformWorldStreamingRequestedState::Activated))
    {
        return Reject(TEXT("WorldStreamingInvalidRequest"), TEXT("位置/时间必须有限，超时为正数，优先级为0到255且状态有效。"));
    }
    if (World->GetShouldForceUnloadStreamingLevels())
    {
        return Reject(TEXT("WorldStreamingExternalChange"), TEXT("世界正在强制卸载，不能新增流送需求。"));
    }

    const FGamePlatformWorldStreamingHandle Handle{ Impl->Generation, FGuid::NewGuid() };
    TUniquePtr<FImpl::FEntry> Entry = MakeUnique<FImpl::FEntry>();
    Entry->Request = Request;
    Entry->PendingSinceSeconds = FPlatformTime::Seconds();
    if (UWorldPartition* Partition = World->GetWorldPartition())
    {
        if (!Request.LevelPackage.IsNone())
        {
            return Reject(TEXT("WorldStreamingInvalidRequest"), TEXT("WP按世界位置请求，不能同时指定传统关卡包。"));
        }
        if (!Partition->IsStreamingEnabled() || (Partition->IsServer() && !Partition->IsServerStreamingEnabled()))
        {
            OutResult = FGamePlatformResult::Unsupported(TEXT("WorldStreamingUnsupportedPartition"),
                TEXT("WP或服务器源流送未启用；本协调器不修改引擎配置，也不把常驻加载冒充源驱动完成。"));
            return {};
        }
        UWorldPartitionSubsystem* Subsystem = World->GetSubsystem<UWorldPartitionSubsystem>();
        if (!Subsystem) { return Reject(TEXT("WorldStreamingSubsystemMissing"), TEXT("当前世界没有WP子系统。")); }
        Entry->Partition = Partition;
        Entry->PartitionSubsystem = Subsystem;
        Entry->Source = MakeUnique<FRequestSource>();
        Entry->Source->World = World;
        Entry->Source->Owner = Request.Owner;
        Entry->Source->Source.Name = FName(*FString::Printf(TEXT("GamePlatformWorld_%s"), *Handle.RequestId.ToString(EGuidFormats::Digits)));
        Entry->Source->Source.Location = Request.TargetLocation;
        Entry->Source->Source.Priority = static_cast<EStreamingSourcePriority>(Request.Priority);
        Entry->Source->Source.TargetState = Request.RequestedState == EGamePlatformWorldStreamingRequestedState::Loaded
            ? EStreamingSourceTargetState::Loaded : EStreamingSourceTargetState::Activated;
        Subsystem->RegisterStreamingSourceProvider(Entry->Source.Get());
        if (!Subsystem->IsStreamingSourceProviderRegistered(Entry->Source.Get()))
        {
            Entry->ReleaseDemand();
            return Reject(TEXT("WorldStreamingRegistrationFailed"), TEXT("WP源注册后核验失败。"));
        }
    }
    else
    {
        if (Request.LevelPackage.IsNone() || !FPackageName::IsValidLongPackageName(Request.LevelPackage.ToString()))
        {
            return Reject(TEXT("WorldStreamingInvalidRequest"), TEXT("传统流送需要已登记关卡的完整包名，不接受短名或对象路径。"));
        }
        const FName CanonicalPackage(*UWorld::RemovePIEPrefix(Request.LevelPackage.ToString()));
        ULevelStreaming* Match = nullptr;
        for (ULevelStreaming* Candidate : World->GetStreamingLevels())
        {
            if (IsValid(Candidate) && FName(*UWorld::RemovePIEPrefix(Candidate->GetWorldAssetPackageName())) == CanonicalPackage)
            {
                if (Match) { return Reject(TEXT("WorldStreamingAmbiguousLevel"), TEXT("同包对应多个已登记关卡，不能按数组顺序选择。")); }
                Match = Candidate;
            }
        }
        if (!Match) { return Reject(TEXT("WorldStreamingLevelNotFound"), TEXT("当前世界未登记目标关卡；不会创建新加载器。")); }
        if (Match->GetIsRequestingUnloadAndRemoval() || Match->GetWorld() != World)
        {
            return Reject(TEXT("WorldStreamingLevelRemoved"), TEXT("目标关卡已请求移除或世界不符。"));
        }
        if (Match->GetLevelStreamingState() == ELevelStreamingState::FailedToLoad)
        {
            return Reject(TEXT("WorldStreamingLoadFailed"), TEXT("目标关卡已加载失败，本请求不重置共享加载器。"));
        }
        Entry->Level = Match;
        // 只向更强需求提升一次，不持续回写，也不使用唯一的PriorityOverride槽破坏他人覆盖。
        Match->SetShouldBeLoaded(true);
        if (Request.RequestedState == EGamePlatformWorldStreamingRequestedState::Activated) { Match->SetShouldBeVisible(true); }
        Match->SetPriority(FMath::Max(Match->GetPriority(), 255 - Request.Priority));
    }
    Impl->Entries.Add(Handle.RequestId, MoveTemp(Entry));
    OutResult = FGamePlatformResult::Success();
    return Handle;
}

FGamePlatformResult FGamePlatformWorldStreaming::Cancel(const FGamePlatformWorldStreamingHandle& Handle)
{
    check(IsInGameThread());
    FImpl::FEntry* Entry = Impl->Find(Handle);
    if (!Entry) { return FGamePlatformResult::Failure(TEXT("WorldStreamingInvalidHandle"), TEXT("句柄不属于当前协调器或代次。")); }
    Entry->ReleaseDemand();
    Entry->Result = { EGamePlatformWorldStreamingState::Cancelled, TEXT("WorldStreamingCancelled") };
    return FGamePlatformResult::Success();
}

void FGamePlatformWorldStreaming::Tick()
{
    check(IsInGameThread());
    if (!Impl->IsOpen()) { Impl->Shutdown(); return; }
    const double NowSeconds = FPlatformTime::Seconds();
    for (auto& Pair : Impl->Entries)
    {
        FImpl::FEntry& Entry = *Pair.Value;
        if (Entry.Result.State != EGamePlatformWorldStreamingState::Pending
            && Entry.Result.State != EGamePlatformWorldStreamingState::Ready) { continue; }
        FName Error;
        const bool bReady = Impl->Sample(Entry, Error);
        if (!Error.IsNone()) { Entry.Fail(Error); continue; }
        if (bReady)
        {
            // Ready不释放需求；仍须持续持源，到显式取消、失败或世界关闭。
            Entry.Result = { EGamePlatformWorldStreamingState::Ready, NAME_None };
        }
        else
        {
            if (Entry.Result.State == EGamePlatformWorldStreamingState::Ready)
            {
                Entry.PendingSinceSeconds = NowSeconds;
                Entry.Result = { EGamePlatformWorldStreamingState::Pending, NAME_None };
            }
            if (NowSeconds - Entry.PendingSinceSeconds >= Entry.Request.TimeoutSeconds)
            {
                Entry.Fail(TEXT("WorldStreamingTimeout"));
            }
        }
    }
}

bool FGamePlatformWorldStreaming::IsRequiredReady() const
{
    check(IsInGameThread());
    if (!Impl->IsOpen()) { return false; }
    for (const auto& Pair : Impl->Entries)
    {
        const FImpl::FEntry& Entry = *Pair.Value;
        if (!Entry.Request.bRequiredForReadiness || Entry.Result.State == EGamePlatformWorldStreamingState::Cancelled) { continue; }
        if (Entry.Result.State != EGamePlatformWorldStreamingState::Ready || !IsOwnerInWorld(Entry.Request.Owner, Impl->World.Get())) { return false; }
    }
    return true;
}

bool FGamePlatformWorldStreaming::HasRequiredFailure() const
{
    check(IsInGameThread());
    for (const auto& Pair : Impl->Entries)
    {
        const FImpl::FEntry& Entry = *Pair.Value;
        if (Entry.Request.bRequiredForReadiness && Entry.Result.State != EGamePlatformWorldStreamingState::Cancelled
            && (Entry.Result.State == EGamePlatformWorldStreamingState::Failed || !IsOwnerInWorld(Entry.Request.Owner, Impl->World.Get()))) { return true; }
    }
    return false;
}

FGamePlatformWorldStreamingResult FGamePlatformWorldStreaming::GetState(const FGamePlatformWorldStreamingHandle& Handle) const
{
    check(IsInGameThread());
    const FImpl::FEntry* Entry = Impl->Find(Handle);
    return Entry ? Entry->Result : FGamePlatformWorldStreamingResult{ EGamePlatformWorldStreamingState::Failed, TEXT("WorldStreamingInvalidHandle") };
}

void FGamePlatformWorldStreaming::Shutdown()
{
    check(IsInGameThread());
    Impl->Shutdown();
}
