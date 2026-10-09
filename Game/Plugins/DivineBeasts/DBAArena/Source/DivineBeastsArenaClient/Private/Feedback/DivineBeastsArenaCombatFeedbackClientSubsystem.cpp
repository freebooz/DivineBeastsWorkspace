// 项目竞技客户端组合根：按本地玩家和当前世界注册命中反馈解析、监听竞技复制事实。
// 只消费平台数据租约与MOBA中立接口；不改变服务器权威战斗结果。
// 激活时绑定回调，世界清理及失活时精确撤销自身回调和租约，避免跨世界遗留。
#include "Feedback/DivineBeastsArenaCombatFeedbackClientSubsystem.h"

#include "Feedback/DivineBeastsArenaCombatFeedbackSettings.h"
#include "Definitions/DivineBeastsCombatFeedbackCatalog.h"
#include "Feedback/GamePlatformHitFeedbackProfile.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "MobaPresentationClientSubsystem.h"
#include "UObject/UObjectGlobals.h"

void UDivineBeastsArenaCombatFeedbackClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UMobaPresentationClientSubsystem>();

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UMobaPresentationClientSubsystem* Moba = LocalPlayer
        ? LocalPlayer->GetSubsystem<UMobaPresentationClientSubsystem>() : nullptr;
    MobaPresentation = Moba;
    if (Moba)
    {
        // 只建立低层中立接口的回调；MOBA不依赖DivineBeasts，不保存强引用到项目对象。
        TWeakObjectPtr<UDivineBeastsArenaCombatFeedbackClientSubsystem> WeakThis(this);
        Moba->SetHitFeedbackResolver(
            [WeakThis](const FGamePlatformCombatEvent& Event,
                       FMobaResolvedHitFeedbackConfiguration& OutConfig)
            {
                if (UDivineBeastsArenaCombatFeedbackClientSubsystem* Self = WeakThis.Get())
                {
                    return Self->ResolveLoadedHitFeedback(Event, OutConfig);
                }
                return false;
            });
    }

    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this, &UDivineBeastsArenaCombatFeedbackClientSubsystem::HandleWorldCleanup);
    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UDivineBeastsArenaCombatFeedbackClientSubsystem::HandlePostLoadMap);

    BeginForWorld(LocalPlayer ? LocalPlayer->GetWorld() : nullptr);
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::Deinitialize()
{
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }
    if (PostLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
        PostLoadMapHandle.Reset();
    }

    // 精确撤销项目组合根的回调；平台及MOBA提供者不受影响。
    if (UMobaPresentationClientSubsystem* Moba = MobaPresentation.Get())
    {
        Moba->ClearHitFeedbackResolver();
    }
    MobaPresentation.Reset();
    CancelWorldLeases();
    Super::Deinitialize();
}

IGamePlatformDataService*
UDivineBeastsArenaCombatFeedbackClientSubsystem::GetDataService() const
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UGameInstance* Instance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
    return Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::BeginForWorld(UWorld* World)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer || !World || LocalPlayer->GetWorld() != World ||
        World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    // 该组合根是竞技专用服务：登录、角色预览、Village和OpenWorld均不请求MOBA打击资产。
    // Client首个PostLoadMap可能早于ArenaGameState复制，此时暂不标记尝试；真正命中时补检查。
    if (!World->GetGameState<AGamePlatformArenaGameState>())
    {
        if (BoundWorld.IsValid() && BoundWorld.Get() != World)
        {
            CancelWorldLeases();
        }
        return;
    }
    if (BoundWorld.Get() == World && bCatalogLoadAttemptedForWorld)
    {
        return; // 包括明确未配置的场景，防止每次命中尝试初始化并重复提交IO。
    }
    CancelWorldLeases();
    BoundWorld = World;
    bCatalogLoadAttemptedForWorld = true;

    const UDivineBeastsArenaCombatFeedbackSettings* Settings =
        GetDefault<UDivineBeastsArenaCombatFeedbackSettings>();
    IGamePlatformDataService* Data = GetDataService();
    if (!Settings || !Settings->CatalogDefinitionId.IsValid() || !Data)
    {
        return; // 尚无正式目录资产时安全关闭个性化反馈，保留平台默认强度。
    }

    const int32 ExpectedGeneration = WorldRequestGeneration;
    TWeakObjectPtr<UDivineBeastsArenaCombatFeedbackClientSubsystem> WeakThis(this);
    FGamePlatformResult RequestResult;
    CatalogLease = Data->AcquireDefinition(
        Settings->CatalogDefinitionId,
        UDivineBeastsCombatFeedbackCatalog::StaticClass(),
        // LocalPlayerSubsystem本身不是World拥有者，使用实例租约并在OnWorldCleanup显式释放。
        TArray<FName>(), EGamePlatformDataLifetime::Instance, this,
        [WeakThis, ExpectedGeneration](
            const FGamePlatformDataLease& CompletedLease,
            const FGamePlatformResult& CompletionResult)
        {
            UDivineBeastsArenaCombatFeedbackClientSubsystem* Self = WeakThis.Get();
            if (!Self || Self->WorldRequestGeneration != ExpectedGeneration ||
                !Self->CatalogLease.IsValid() ||
                Self->CatalogLease.LeaseId != CompletedLease.LeaseId)
            {
                return;
            }
            if (!CompletionResult.IsSuccess())
            {
                // 失败的请求保持当前World已尝试标记；直至切图才允许重试。
                // 不在回调中移除世界身份，避免战斗热路径重复异步提交失败请求。
                if (IGamePlatformDataService* ActiveData = Self->GetDataService())
                {
                    ActiveData->ReleaseDefinition(Self->CatalogLease);
                }
                Self->CatalogLease = FGamePlatformDataLease{};
                return;
            }

            // 在目录加载完成时预热前64个唯一Profile；实际命中路径只读已经加载的对象。
            // 内容优先级由目录条目顺序确定，不依赖TMap/TSet不确定的迭代顺序。
            IGamePlatformDataService* ActiveData = Self->GetDataService();
            const UDivineBeastsCombatFeedbackCatalog* LoadedCatalog = ActiveData
                ? Cast<UDivineBeastsCombatFeedbackCatalog>(
                    ActiveData->GetLoadedDefinition(Self->CatalogLease))
                : nullptr;
            if (!LoadedCatalog)
            {
                return;
            }

            TSet<FPrimaryAssetId> SeenDefinitions;
            for (const FDivineBeastsCombatFeedbackEntry& Entry : LoadedCatalog->Entries)
            {
                if (!Entry.ProfileDefinitionId.IsValid() ||
                    SeenDefinitions.Contains(Entry.ProfileDefinitionId))
                {
                    continue;
                }
                SeenDefinitions.Add(Entry.ProfileDefinitionId);
                if (SeenDefinitions.Num() > MaxActiveProfileLeases)
                {
                    break; // 超出本地预算的配置本世界使用通用反馈，禁止无限申请或缓存。
                }
                Self->BeginProfileLoad(Entry.ProfileDefinitionId);
            }
        }, RequestResult);
}

bool UDivineBeastsArenaCombatFeedbackClientSubsystem::ResolveLoadedHitFeedback(
    const FGamePlatformCombatEvent& Event,
    FMobaResolvedHitFeedbackConfiguration& OutConfig)
{
    // 初始加载或Travel时本地玩家World切换事件可能晚于地图回调；首次命中可补一次异步挂载。
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* CurrentWorld = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    if (CurrentWorld && CurrentWorld != BoundWorld.Get())
    {
        BeginForWorld(CurrentWorld);
    }
    UWorld* World = BoundWorld.Get();
    IGamePlatformDataService* Data = GetDataService();
    if (!World || !Data || !Event.SourceActor || Event.SourceAbilityId.IsNone() ||
        Event.SourceActor->GetWorld() != World || !CatalogLease.IsValid())
    {
        return false;
    }

    const UDivineBeastsCombatFeedbackCatalog* Catalog =
        Cast<UDivineBeastsCombatFeedbackCatalog>(
            Data->GetLoadedDefinition(CatalogLease));
    if (!Catalog)
    {
        return false; // 目录未完成异步加载，绝不同步读取磁盘或回退项目硬编码路径。
    }

    const UDivineBeastsCharacterComponent* Identity =
        Event.SourceActor->FindComponentByClass<UDivineBeastsCharacterComponent>();
    if (!Identity || Identity->GetHeroDefinitionId().IsNone() ||
        (Event.SourceAvatarGeneration > 0 &&
         Event.SourceAvatarGeneration != Identity->GetAvatarGeneration()))
    {
        return false;
    }

    FDivineBeastsCombatFeedbackEntry Entry;
    if (!Catalog->TryResolve(
        Identity->GetHeroDefinitionId(), Event.SourceAbilityId, Entry) ||
        !Entry.ProfileDefinitionId.IsValid())
    {
        return false;
    }

    const FGamePlatformDataLease* Existing = ProfileLeases.Find(Entry.ProfileDefinitionId);
    if (!Existing)
    {
        BeginProfileLoad(Entry.ProfileDefinitionId);
        return false; // 首次未预加载时使用MOBA默认反馈，后续命中使用特化配置。
    }

    const UGamePlatformHitFeedbackProfile* LoadedProfile =
        Cast<UGamePlatformHitFeedbackProfile>(
            Data->GetLoadedDefinition(*Existing));
    if (!LoadedProfile)
    {
        return false;
    }

    OutConfig.LoadedProfile = const_cast<UGamePlatformHitFeedbackProfile*>(LoadedProfile);
    OutConfig.VFXDefinitionId = Entry.VFXDefinitionId;
    OutConfig.SFXDefinitionId = Entry.SFXDefinitionId;
    return true;
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::BeginProfileLoad(
    const FPrimaryAssetId& ProfileDefinitionId)
{
    if (ProfileLeases.Contains(ProfileDefinitionId) ||
        FailedProfiles.Contains(ProfileDefinitionId) ||
        ProfileLeases.Num() >= MaxActiveProfileLeases)
    {
        return;
    }
    IGamePlatformDataService* Data = GetDataService();
    if (!Data || !ProfileDefinitionId.IsValid())
    {
        return;
    }

    const int32 ExpectedGeneration = WorldRequestGeneration;
    TWeakObjectPtr<UDivineBeastsArenaCombatFeedbackClientSubsystem> WeakThis(this);
    FGamePlatformResult RequestResult;
    FGamePlatformDataLease Lease = Data->AcquireDefinition(
        ProfileDefinitionId,
        UGamePlatformHitFeedbackProfile::StaticClass(),
        // Client软资源只在该本地玩家需要时加载；世界清理时主动释放实例租约。
        TArray<FName>{FName(TEXT("Client"))}, EGamePlatformDataLifetime::Instance, this,
        [WeakThis, ProfileDefinitionId, ExpectedGeneration](
            const FGamePlatformDataLease& CompletedLease,
            const FGamePlatformResult& CompletionResult)
        {
            UDivineBeastsArenaCombatFeedbackClientSubsystem* Self = WeakThis.Get();
            if (!Self || Self->WorldRequestGeneration != ExpectedGeneration)
            {
                return;
            }
            const FGamePlatformDataLease* Pending = Self->ProfileLeases.Find(ProfileDefinitionId);
            if (!Pending || Pending->LeaseId != CompletedLease.LeaseId)
            {
                return;
            }
            if (!CompletionResult.IsSuccess())
            {
                // 加载失败的技能当前世界不重试，避免高速攻击每次申请IO。
                Self->FailedProfiles.Add(ProfileDefinitionId);
                if (IGamePlatformDataService* ActiveData = Self->GetDataService())
                {
                    ActiveData->ReleaseDefinition(*Pending);
                }
                Self->ProfileLeases.Remove(ProfileDefinitionId);
            }
        }, RequestResult);

    if (!Lease.IsValid() || !RequestResult.IsSuccess())
    {
        FailedProfiles.Add(ProfileDefinitionId);
        return;
    }
    ProfileLeases.Add(ProfileDefinitionId, MoveTemp(Lease));
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::CancelWorldLeases()
{
    ++WorldRequestGeneration;
    if (IGamePlatformDataService* Data = GetDataService())
    {
        for (const TPair<FPrimaryAssetId, FGamePlatformDataLease>& Pair : ProfileLeases)
        {
            Data->ReleaseDefinition(Pair.Value);
        }
        if (CatalogLease.IsValid())
        {
            Data->ReleaseDefinition(CatalogLease);
        }
    }
    ProfileLeases.Reset();
    FailedProfiles.Reset();
    CatalogLease = FGamePlatformDataLease{};
    BoundWorld.Reset();
    bCatalogLoadAttemptedForWorld = false;
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::HandlePostLoadMap(UWorld* World)
{
    BeginForWorld(World);
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::HandleWorldCleanup(
    UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
    if (World && BoundWorld.Get() == World)
    {
        CancelWorldLeases();
    }
}
