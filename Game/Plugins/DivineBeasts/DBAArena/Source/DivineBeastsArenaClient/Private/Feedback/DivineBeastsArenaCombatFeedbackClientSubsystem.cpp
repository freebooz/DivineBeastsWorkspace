// 项目竞技客户端组合根：按本地玩家和当前世界注册命中反馈解析、监听竞技复制事实。
// 只消费平台数据租约与MOBA中立接口；不改变服务器权威战斗结果。
// 激活时绑定回调，世界清理及失活时精确撤销自身回调和租约，避免跨世界遗留。
#include "Feedback/DivineBeastsArenaCombatFeedbackClientSubsystem.h"

#include "Feedback/DivineBeastsArenaCombatFeedbackSettings.h"
#include "Feedback/DivineBeastsHitFeedbackGrantPolicy.h"

#include "Definitions/DivineBeastsCombatFeedbackCatalog.h"
#include "Feedback/GamePlatformHitFeedbackProfile.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "MobaPresentationClientSubsystem.h"
#include "Adapters/Combat/DivineBeastsCombatUIFeedbackLibrary.h"
#include "Feedback/GamePlatformFeedbackWidget.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Subsystems/GamePlatformCombatFeedbackWorldSubsystem.h"
#include "Engine/StreamableManager.h"
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

    WatchWorld(LocalPlayer ? LocalPlayer->GetWorld() : nullptr);
    if (LocalPlayer && LocalPlayer->GetWorld())
    {
        PlayerControllerChanged(
            LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()));
    }
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
    PlayerControllerChanged(nullptr);
    UnwatchWorld();
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

void UDivineBeastsArenaCombatFeedbackClientSubsystem::PlayerControllerChanged(
    APlayerController* NewController)
{
    Super::PlayerControllerChanged(NewController);

    if (IsValid(NewController) && ObservedController.Get() == NewController)
    {
        // 同一Controller的重复通知不撤销再申请Profile，保持已加载资源和订阅稳定。
        WatchWorld(NewController->GetWorld());
        BindLocalLoadout(NewController->GetPawn());
        return;
    }

    if (APlayerController* OldController = ObservedController.Get())
    {
        OldController->OnPossessedPawnChanged.RemoveDynamic(
            this,
            &UDivineBeastsArenaCombatFeedbackClientSubsystem::HandlePossessedPawnChanged);
    }
    UnbindLocalLoadout();

    // 切换Controller或失去本地拥有者时释放旧角色资源。
    // 原实现只Unbind，空Pawn时会跳过BindLocalLoadout中的释放路径，导致旧Profile持续占用内存。
    ReleaseProfileLeases();
    ObservedController = IsValid(NewController) ? NewController : nullptr;

    if (IsValid(NewController))
    {
        NewController->OnPossessedPawnChanged.AddUniqueDynamic(
            this,
            &UDivineBeastsArenaCombatFeedbackClientSubsystem::HandlePossessedPawnChanged);
        WatchWorld(NewController->GetWorld());
        BindLocalLoadout(NewController->GetPawn());
    }
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::WatchWorld(UWorld* World)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!World || !LocalPlayer || LocalPlayer->GetWorld() != World ||
        World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (ObservedWorld.Get() != World)
    {
        // 世界指针变化时先撤销旧世界的观察句柄和异步资源租约。
        UnwatchWorld();
        CancelWorldLeases();
        ObservedWorld = World;
        GameStateSetHandle = World->GameStateSetEvent.AddUObject(
            this, &UDivineBeastsArenaCombatFeedbackClientSubsystem::HandleGameStateSet);
    }

    // 客户端GameState可能晚于PostLoadMap复制；同一World只进行一次竞技资格判断。
    // 未确认竞技时不加载任何竞技表现资产。
    if (World->GetGameState<AGamePlatformArenaGameState>())
    {
        BeginForWorld(World);
        BindWorldCombatFeedback(*World);
        BeginFloatingTextWidgetLoad();
    }
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::UnwatchWorld()
{
    if (UWorld* World = ObservedWorld.Get())
    {
        if (GameStateSetHandle.IsValid())
        {
            World->GameStateSetEvent.Remove(GameStateSetHandle);
        }
    }
    UnbindWorldCombatFeedback();
    GameStateSetHandle.Reset();
    ObservedWorld.Reset();
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::BindWorldCombatFeedback(UWorld& World)
{
    UGamePlatformCombatFeedbackWorldSubsystem* Bus =
        World.GetSubsystem<UGamePlatformCombatFeedbackWorldSubsystem>();
    if (BoundFeedbackWorldBus.Get() == Bus)
    {
        return;
    }
    UnbindWorldCombatFeedback();
    if (Bus)
    {
        BoundFeedbackWorldBus = Bus;
        FeedbackWorldBusHandle = Bus->OnConfirmedFeedback().AddUObject(
            this, &UDivineBeastsArenaCombatFeedbackClientSubsystem::HandleConfirmedCombatFeedback);
    }
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::UnbindWorldCombatFeedback()
{
    if (UGamePlatformCombatFeedbackWorldSubsystem* Bus = BoundFeedbackWorldBus.Get())
    {
        if (FeedbackWorldBusHandle.IsValid())
        {
            Bus->OnConfirmedFeedback().Remove(FeedbackWorldBusHandle);
        }
    }
    FeedbackWorldBusHandle.Reset();
    BoundFeedbackWorldBus.Reset();
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::BeginFloatingTextWidgetLoad()
{
    if (bFloatingTextWidgetLoadRequested || !BoundWorld.IsValid())
    {
        return;
    }

    const UDivineBeastsArenaCombatFeedbackSettings* Settings =
        GetDefault<UDivineBeastsArenaCombatFeedbackSettings>();
    if (!Settings || Settings->FloatingTextWidgetClass.IsNull())
    {
        bFloatingTextWidgetLoadRequested = true;
        return; // 没有真实Widget资产时不创建假的UI控件，也不影响攻击反馈。
    }

    const FSoftObjectPath WidgetPath = Settings->FloatingTextWidgetClass.ToSoftObjectPath();
    if (!WidgetPath.IsValid())
    {
        bFloatingTextWidgetLoadRequested = true;
        return;
    }

    bFloatingTextWidgetLoadRequested = true;
    const int32 ExpectedGeneration = WorldRequestGeneration;
    TArray<FSoftObjectPath> AssetsToLoad;
    AssetsToLoad.Add(WidgetPath);
    FloatingTextWidgetLoadHandle = FGamePlatformAssetLoader::RequestAsyncLoad(
        AssetsToLoad,
        FStreamableDelegate::CreateUObject(
            this,
            &UDivineBeastsArenaCombatFeedbackClientSubsystem::HandleFloatingTextWidgetLoaded,
            ExpectedGeneration));
    if (!FloatingTextWidgetLoadHandle.IsValid())
    {
        // 请求失败不按每一条命中再次加载；地图改变后可重新尝试。
        return;
    }
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::HandleFloatingTextWidgetLoaded(
    int32 ExpectedWorldRequestGeneration)
{
    if (WorldRequestGeneration != ExpectedWorldRequestGeneration || !BoundWorld.IsValid())
    {
        return;
    }
    const UDivineBeastsArenaCombatFeedbackSettings* Settings =
        GetDefault<UDivineBeastsArenaCombatFeedbackSettings>();
    UClass* WidgetType = Settings ? Settings->FloatingTextWidgetClass.Get() : nullptr;
    if (IsValid(WidgetType) &&
        WidgetType->IsChildOf(UGamePlatformFeedbackWidget::StaticClass()) &&
        !WidgetType->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
    {
        LoadedFloatingTextWidgetClass = WidgetType;
    }
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::HandleConfirmedCombatFeedback(
    const FGamePlatformCombatEvent& Event)
{
    // 世界网络事件已由权威Combat单向转发；本桥仅做只读DTO→平台UI反馈。
    // 绝不调用Damage/ASC或添加第二套World Widget管理器。
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = BoundWorld.Get();
    UClass* WidgetType = LoadedFloatingTextWidgetClass.Get();
    if (!IsInGameThread() || !LocalPlayer || !World ||
        LocalPlayer->GetWorld() != World ||
        !IsValid(WidgetType) || !IsValid(Event.TargetActor) ||
        Event.TargetActor->GetWorld() != World || !Event.EventId.IsValid())
    {
        return;
    }

    // 不直接传递Gameplay对象或账号身份，UI只消费一次性的已确认展示数字。
    FDivineBeastsCombatFeedbackInput Input;
    Input.EventId = Event.EventId;
    Input.TargetVisualKey = Event.TargetActor->GetFName();
    Input.WorldLocation = Event.ImpactPoint.ContainsNaN()
        ? Event.TargetActor->GetActorLocation() : FVector(Event.ImpactPoint);

    const TSubclassOf<UGamePlatformFeedbackWidget> FeedbackWidgetClass(WidgetType);
    auto Submit = [&Input, LocalPlayer, FeedbackWidgetClass](
        EDivineBeastsCombatFeedbackKind Kind, double Magnitude, uint32 ChannelSalt)
    {
        if (!FMath::IsFinite(Magnitude) || Magnitude < 0.0)
        {
            return;
        }
        Input.Kind = Kind;
        Input.Magnitude = Magnitude;
        Input.EventId.A ^= ChannelSalt;
        UDivineBeastsCombatUIFeedbackLibrary::SubmitFloatingText(
            LocalPlayer, Input, FeedbackWidgetClass);
        Input.EventId.A ^= ChannelSalt;
    };

    switch (Event.EventType)
    {
    case EGamePlatformCombatEventType::Damage:
        if (Event.AppliedToHealth > 0.0f)
        {
            Submit(EDivineBeastsCombatFeedbackKind::Damage, Event.AppliedToHealth, 0u);
        }
        if (Event.AppliedToShield > 0.0f)
        {
            // Damage与ShieldDamage同源但视觉样式独立；稳定ChannelSalt防止UI去重互相吞并。
            Submit(EDivineBeastsCombatFeedbackKind::ShieldDamage,
                Event.AppliedToShield, 0x53484C44u);
        }
        break;
    case EGamePlatformCombatEventType::Healing:
        Submit(EDivineBeastsCombatFeedbackKind::Healing, Event.AppliedMagnitude, 0x4845414Cu);
        break;
    case EGamePlatformCombatEventType::Death:
        Submit(EDivineBeastsCombatFeedbackKind::Death, 0.0, 0x44454154u);
        break;
    default:
        break;
    }
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::HandleGameStateSet(
    AGameStateBase* GameState)
{
    if (!IsValid(GameState) || GameState->GetWorld() != ObservedWorld.Get())
    {
        return;
    }
    if (GameState->IsA<AGamePlatformArenaGameState>())
    {
        BeginForWorld(GameState->GetWorld());
        BindWorldCombatFeedback(*GameState->GetWorld());
        BeginFloatingTextWidgetLoad();
    }
    else if (BoundWorld.Get() == GameState->GetWorld())
    {
        // 竞技状态被普通世界替换时，停止UI反馈并释放旧竞技Profile与目录。
        UnbindWorldCombatFeedback();
        CancelWorldLeases();
    }
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::HandlePossessedPawnChanged(
    APawn* /*PreviousPawn*/, APawn* NewPawn)
{
    if (IsValid(NewPawn))
    {
        WatchWorld(NewPawn->GetWorld());
    }
    BindLocalLoadout(NewPawn);
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::BindLocalLoadout(APawn* NewPawn)
{
    UWorld* World = ObservedWorld.Get();
    UDivineBeastsAbilityLoadoutComponent* NewLoadout =
        IsValid(NewPawn) && NewPawn->GetWorld() == World
            ? NewPawn->FindComponentByClass<UDivineBeastsAbilityLoadoutComponent>()
            : nullptr;
    if (ObservedLoadout.Get() == NewLoadout)
    {
        // 旧Pawn已销毁导致弱引用转空时，也不能让该英雄的Profile租约继续存活。
        if (NewLoadout)
        {
            PrewarmAuthorizedLocalAbilities();
        }
        else
        {
            ReleaseProfileLeases();
        }
        return;
    }

    UnbindLocalLoadout();
    ReleaseProfileLeases(); // 角色/Pawn切换后不保留旧英雄已预加载资源，占用按需受控。
    ObservedLoadout = NewLoadout;
    if (IsValid(NewLoadout))
    {
        LoadoutChangedHandle = NewLoadout->OnLoadoutChanged().AddUObject(
            this, &UDivineBeastsArenaCombatFeedbackClientSubsystem::HandleLoadoutChanged);
        PrewarmAuthorizedLocalAbilities();
    }
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::UnbindLocalLoadout()
{
    if (UDivineBeastsAbilityLoadoutComponent* Loadout = ObservedLoadout.Get())
    {
        if (LoadoutChangedHandle.IsValid())
        {
            Loadout->OnLoadoutChanged().Remove(LoadoutChangedHandle);
        }
    }
    LoadoutChangedHandle.Reset();
    ObservedLoadout.Reset();
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::HandleLoadoutChanged(
    const FDivineBeastsAbilityLoadoutState& /*State*/)
{
    // 客户端只观察OwnerOnly授予结果，不在此授权、激活或推导技能。
    PrewarmAuthorizedLocalAbilities();
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::PrewarmAuthorizedLocalAbilities()
{
    const UDivineBeastsAbilityLoadoutComponent* Loadout = ObservedLoadout.Get();
    IGamePlatformDataService* Data = GetDataService();
    UWorld* World = BoundWorld.Get();
    if (!IsValid(Loadout) || !Data || !World || !CatalogLease.IsValid() ||
        !IsValid(Loadout->GetOwner()) || Loadout->GetOwner()->GetWorld() != World)
    {
        return;
    }

    const UDivineBeastsCombatFeedbackCatalog* Catalog =
        Cast<UDivineBeastsCombatFeedbackCatalog>(
            Data->GetLoadedDefinition(CatalogLease));
    if (!Catalog)
    {
        return; // 目录尚未完成异步加载，由完成回调再触发一次，不进行同步IO。
    }

    const FDivineBeastsAbilityLoadoutState& State = Loadout->GetLoadoutStateRef();
    const UDivineBeastsCharacterComponent* Identity =
        Loadout->GetOwner()->FindComponentByClass<UDivineBeastsCharacterComponent>();
    if (!State.bReady || State.HeroDefinitionId.IsNone() ||
        State.AvatarGeneration <= 0 || !Identity ||
        Identity->GetHeroDefinitionId() != State.HeroDefinitionId ||
        Identity->GetAvatarGeneration() != State.AvatarGeneration)
    {
        // 服务器撤销授予或角色尚未就绪时，不能延续上一代角色的反馈资产。
        if (!AuthorizedLocalProfileIds.IsEmpty() ||
            !AuthorizedHeroDefinitionId.IsNone() || AuthorizedAvatarGeneration > 0)
        {
            ReleaseProfileLeases();
        }
        return;
    }

    // 先根据OwnerOnly授予快照构建完整集合，再决定是否需要释放旧租约。
    TSet<FPrimaryAssetId> DesiredProfiles;
    TArray<FPrimaryAssetId> OrderedProfiles;
    for (const FDivineBeastsGrantedAbilitySlot& Slot : State.Slots)
    {
        if (Slot.AbilityId.IsNone() || Slot.AbilityLevel <= 0)
        {
            continue;
        }
        FDivineBeastsCombatFeedbackEntry Entry;
        if (!Catalog->TryResolve(State.HeroDefinitionId, Slot.AbilityId, Entry) ||
            !Entry.ProfileDefinitionId.IsValid() ||
            DesiredProfiles.Contains(Entry.ProfileDefinitionId))
        {
            continue;
        }
        DesiredProfiles.Add(Entry.ProfileDefinitionId);
        OrderedProfiles.Add(Entry.ProfileDefinitionId);
    }

    const bool bAuthorizationChanged =
        DivineBeastsHitFeedbackGrantPolicy::RequiresRefresh(
            AuthorizedLocalProfileIds, AuthorizedHeroDefinitionId, AuthorizedAvatarGeneration,
            DesiredProfiles, State.HeroDefinitionId, State.AvatarGeneration);

    if (bAuthorizationChanged)
    {
        // 技能授权被撤销或角色代次变化时清理旧租约，避免旧技能长期占用客户端资源。
        ReleaseProfileLeases();
        AuthorizedLocalProfileIds = MoveTemp(DesiredProfiles);
        AuthorizedHeroDefinitionId = State.HeroDefinitionId;
        AuthorizedAvatarGeneration = State.AvatarGeneration;
    }

    // 按服务器授予槽位顺序预热，预算不足时不会遍历整个生肖目录申请大量资源。
    for (const FPrimaryAssetId& Id : OrderedProfiles)
    {
        if (ProfileLeases.Num() >= MaxActiveProfileLeases &&
            !ProfileLeases.Contains(Id))
        {
            break;
        }
        BeginProfileLoad(Id);
    }
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
    if (BoundWorld.Get() != World)
    {
        // 仅在真正切换World时失效旧异步请求；禁止同一World的重复检查取消浮字句柄。
        CancelWorldLeases();
        BoundWorld = World;
    }
    if (bCatalogLoadAttemptedForWorld)
    {
        return; // 同世界已有有效申请或明确没有配置，禁止重复向数据服务请求。
    }

    const UDivineBeastsArenaCombatFeedbackSettings* Settings =
        GetDefault<UDivineBeastsArenaCombatFeedbackSettings>();
    if (!Settings || !Settings->CatalogDefinitionId.IsValid())
    {
        // 编辑器尚无真正Catalog资产时永久降级本场竞技通用表现，不编造主资产身份。
        bCatalogLoadAttemptedForWorld = true;
        return;
    }
    IGamePlatformDataService* Data = GetDataService();
    if (!Data)
    {
        // 数据服务可能晚于ArenaGameState就绪：不提前标记已申请，也不取消UI租约。
        // 后续GameState/Pawn/命中事件可重新检查，无效服务状态不会发起磁盘IO。
        return;
    }

    bCatalogLoadAttemptedForWorld = true;
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

            // 目录就绪后只预热当前LocalPlayer已由服务器授予的真实技能。
            // 其他敌方技能在收到首次确认命中后按需申请，避免加载十二生肖全部资源。
            Self->PrewarmAuthorizedLocalAbilities();
        }, RequestResult);
}

bool UDivineBeastsArenaCombatFeedbackClientSubsystem::ResolveLoadedHitFeedback(
    const FGamePlatformCombatEvent& Event,
    FMobaResolvedHitFeedbackConfiguration& OutConfig)
{
    // 初始加载或Travel时本地玩家World切换事件可能晚于地图回调；首次命中可补一次异步挂载。
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* CurrentWorld = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    if (CurrentWorld)
    {
        WatchWorld(CurrentWorld); // 已观察同一World时O(1)，无需扫描全部战斗Actor。
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

void UDivineBeastsArenaCombatFeedbackClientSubsystem::ReleaseProfileLeases()
{
    if (IGamePlatformDataService* Data = GetDataService())
    {
        for (const TPair<FPrimaryAssetId, FGamePlatformDataLease>& Pair : ProfileLeases)
        {
            Data->ReleaseDefinition(Pair.Value);
        }
    }
    // 释放之后收到异步加载回调时，因请求句柄不再在Map中，不会污染新英雄。
    ProfileLeases.Reset();
    FailedProfiles.Reset();
    AuthorizedLocalProfileIds.Reset();
    AuthorizedHeroDefinitionId = NAME_None;
    AuthorizedAvatarGeneration = 0;
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::CancelWorldLeases()
{
    ++WorldRequestGeneration;
    FGamePlatformAssetLoader::Cancel(FloatingTextWidgetLoadHandle);
    FloatingTextWidgetLoadHandle.Reset();
    LoadedFloatingTextWidgetClass.Reset();
    bFloatingTextWidgetLoadRequested = false;
    if (IGamePlatformDataService* Data = GetDataService())
    {
        if (CatalogLease.IsValid())
        {
            Data->ReleaseDefinition(CatalogLease);
        }
    }
    ReleaseProfileLeases();
    CatalogLease = FGamePlatformDataLease{};
    BoundWorld.Reset();
    bCatalogLoadAttemptedForWorld = false;
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::HandlePostLoadMap(UWorld* World)
{
    WatchWorld(World);
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (World && LocalPlayer->GetWorld() == World)
        {
            PlayerControllerChanged(LocalPlayer->GetPlayerController(World));
        }
    }
}

void UDivineBeastsArenaCombatFeedbackClientSubsystem::HandleWorldCleanup(
    UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
    if (!World || (ObservedWorld.Get() != World && BoundWorld.Get() != World))
    {
        return;
    }
    // Pawn/World退出先注销本地所有者授权订阅，不能再回放旧角色输入或旧资源。
    PlayerControllerChanged(nullptr);
    UnwatchWorld();
    CancelWorldLeases();
}
