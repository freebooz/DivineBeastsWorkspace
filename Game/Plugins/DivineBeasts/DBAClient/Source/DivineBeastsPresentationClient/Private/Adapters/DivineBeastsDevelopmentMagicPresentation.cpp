// 项目纯客户端适配：订阅真实复制弹体事实，经目录/Data/平台中立请求播放；不导入Niagara执行器。
// 同一LocalPlayer独占注册/请求/订阅；世界退出先取消活跃请求再撤销目录及自己的租约，无业务Tick。
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
/** 允许列表仅解释项目三种开发分类；不让复制事实携带或拼接任意资源路径。 */
FName MagicName(FName Style)
{
    if (Style == TEXT("DBA.Development.Fireball")) return TEXT("Fireball");
    if (Style == TEXT("DBA.Development.Frostbolt")) return TEXT("Frostbolt");
    if (Style == TEXT("DBA.Development.ArcaneMissiles")) return TEXT("ArcaneMissiles");
    return NAME_None;
}
FGameplayTag MagicSemantic(FName Name, bool bImpact)
{
    return FGameplayTag::RequestGameplayTag(FName(*(TEXT("Presentation.DivineBeasts.Development.Magic.") +
        Name.ToString() + (bImpact ? TEXT(".Impact") : TEXT(".Flight")))), false);
}
}

void UDivineBeastsPresentationClientSubsystem::BindDevelopmentMagicWorld(UWorld* World)
{
    if (DevelopmentMagicWorld.Get() == World && DevelopmentMagicPack.IsValid()) return;
    UnbindDevelopmentMagicWorld();
    if (bClosing || !World || World->bIsTearingDown || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer ||
        !GetLocalPlayer() || GetLocalPlayer()->GetWorld() != World ||
        !UDivineBeastsDevelopmentMagicDefinition::IsDevelopmentEnabledForCurrentProcess()) return;
    DevelopmentMagicWorld = World;
    DevelopmentMagicSpawnHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleDevelopmentMagicSpawn));
    DevelopmentMagicPackHandle = ContentPackChanged.AddUObject(this, &ThisClass::HandleDevelopmentMagicPack);
    FDivineBeastsPresentationContentPackFragment Pack;
    Pack.ContentPackId = TEXT("DBA.Development.Magic"); Pack.Revision = 1;
    Pack.LifecycleScope = EGamePlatformPresentationContextScope::World; Pack.bRequiredPreload = true;
    auto& Catalog = Pack.CatalogFragment;
    Catalog.FragmentId = Pack.ContentPackId; Catalog.OwnerScopeId = Pack.ContentPackId;
    Catalog.Revision = 1; Catalog.Scope = EGamePlatformPresentationCatalogScope::ContentPack; Catalog.LifecycleScope = Pack.LifecycleScope;
    for (const TCHAR* Name : {TEXT("Fireball"), TEXT("Frostbolt"), TEXT("ArcaneMissiles")})
        for (bool bImpact : {false, true})
        {
            FGamePlatformPresentationCatalogEntry Entry;
            Entry.EntryId = FName(*(FString(TEXT("DBA.Development.Magic.")) + Name + (bImpact ? TEXT(".Impact") : TEXT(".Flight"))));
            Entry.SemanticTag = MagicSemantic(Name, bImpact); Entry.ProviderChannel = TEXT("VFX");
            Entry.DefinitionId = FName(*(FString(TEXT("presentation.dba.development.magic.")) + FString(Name).ToLower() +
                (bImpact ? TEXT(".impact@1") : TEXT(".flight@1"))));
            Entry.ContextQuery.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
            Entry.Scope = Catalog.Scope; Entry.Specificity = 1; Entry.ContentRevision = TEXT("1");
            Catalog.Entries.Add(Entry);
        }
    FString Error;
    const auto Handle = ActivateContentPack(Pack, Error, &DevelopmentMagicPack);
    if (!Handle.IsValid()) { UnbindDevelopmentMagicWorld(); return; }
    DevelopmentMagicPack = Handle;
    for (TActorIterator<AGamePlatformCombatProjectile> It(World); It; ++It) HandleDevelopmentMagicSpawn(*It);
}

void UDivineBeastsPresentationClientSubsystem::HandleDevelopmentMagicSpawn(AActor* Actor)
{
    auto* Projectile = Cast<AGamePlatformCombatProjectile>(Actor);
    if (bClosing || !Projectile || Projectile->GetWorld() != DevelopmentMagicWorld.Get() ||
        DevelopmentMagicFlights.Contains(Projectile)) return;
    FDevelopmentMagicFlight Flight;
    Flight.FactsHandle = Projectile->OnFlightFactsChanged().AddUObject(this, &ThisClass::HandleDevelopmentMagicFacts);
    DevelopmentMagicFlights.Add(Projectile, Flight);
    HandleDevelopmentMagicFacts(Projectile, Projectile->GetFlightFacts());
}

void UDivineBeastsPresentationClientSubsystem::HandleDevelopmentMagicPack(
    const FDivineBeastsPresentationContentPackHandle& Handle, EDivineBeastsPresentationContentPackState State, const FString& Error)
{
    if (bClosing || Handle.Id != DevelopmentMagicPack.Id || Handle.Generation != DevelopmentMagicPack.Generation ||
        State != EDivineBeastsPresentationContentPackState::Active) return;
    // 只复核仍存活的Actor当前状态；不排队复活过期瞬时命中或旧世界效果。
    TArray<TWeakObjectPtr<AGamePlatformCombatProjectile>> Actors;
    DevelopmentMagicFlights.GetKeys(Actors);
    for (auto& Weak : Actors) if (auto* Actor = Weak.Get()) HandleDevelopmentMagicFacts(Actor, Actor->GetFlightFacts());
}

void UDivineBeastsPresentationClientSubsystem::HandleDevelopmentMagicFacts(
    AGamePlatformCombatProjectile* Actor, const FGamePlatformCombatProjectileFacts& Facts)
{
    auto* Flight = DevelopmentMagicFlights.Find(Actor);
    if (bClosing || !IsValid(Actor) || !Flight || Actor->GetWorld() != DevelopmentMagicWorld.Get() ||
        !Facts.bStarted || !Facts.OccurrenceId.IsValid() || Facts.SourceAvatarGeneration <= 0) return;
    const FName Name = MagicName(Facts.PresentationStyleId);
    if (Name.IsNone()) return;
    Flight->StyleId = Facts.PresentationStyleId;
    const auto CancelFlight = [this, Flight, Name]()
    {
        if (!Flight->FlightRequestId.IsValid()) return;
        FGamePlatformPresentationRequest Cancel;
        Cancel.RequestId = Flight->FlightRequestId; Cancel.SemanticTag = MagicSemantic(Name, false);
        Cancel.PredictionState = EGamePlatformPresentationPredictionState::Cancelled;
        SubmitProjectRequest(Cancel); Flight->FlightRequestId.Invalidate();
    };
    if (Facts.bFinished) { CancelFlight(); Flight->bFlightEnded = true; }
    FString Error;
    if (GetContentPackState(DevelopmentMagicPack, Error) != EDivineBeastsPresentationContentPackState::Active) return;
    const auto Submit = [this, Actor, &Facts, Name](bool bImpact)
    {
        FGamePlatformPresentationRequest Request;
        Request.RequestId = FGuid::NewGuid(); Request.SemanticTag = MagicSemantic(Name, bImpact);
        Request.ContextId = Facts.PresentationStyleId; Request.SourceId = Facts.AbilityId;
        Request.SourceLocation = bImpact ? Facts.ImpactLocation : Actor->GetActorLocation();
        Request.ImpactLocation = Facts.ImpactLocation; Request.ImpactNormal = Facts.ImpactNormal;
        Request.Context.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
        Request.Context.AvatarGeneration = Facts.SourceAvatarGeneration;
        Request.RequestGeneration = Facts.SourceAvatarGeneration;
        Request.WorldGeneration = PlatformPresentation ? PlatformPresentation->GetWorldGeneration() : 0;
        Request.Lifetime = bImpact ? EGamePlatformPresentationLifetime::Timed : EGamePlatformPresentationLifetime::Persistent;
        if (!bImpact) Request.AttachComponent = Actor->GetRootComponent();
        return SubmitProjectRequest(Request) == EGamePlatformPresentationSubmitResult::Submitted ? Request.RequestId : FGuid();
    };
    if (!Facts.bFinished && !Flight->bFlightEnded && !Flight->FlightRequestId.IsValid()) Flight->FlightRequestId = Submit(false);
    if (Facts.bFinished && Facts.bImpacted && !Flight->bImpactSubmitted && !Actor->IsActorBeingDestroyed())
        Flight->bImpactSubmitted = Submit(true).IsValid();
    if (Actor->IsActorBeingDestroyed())
    { Actor->OnFlightFactsChanged().Remove(Flight->FactsHandle); DevelopmentMagicFlights.Remove(Actor); }
}

void UDivineBeastsPresentationClientSubsystem::UnbindDevelopmentMagicWorld()
{
    if (UWorld* World = DevelopmentMagicWorld.Get()) World->RemoveOnActorSpawnedHandler(DevelopmentMagicSpawnHandle);
    DevelopmentMagicSpawnHandle.Reset();
    ContentPackChanged.Remove(DevelopmentMagicPackHandle); DevelopmentMagicPackHandle.Reset();
    for (auto& Pair : DevelopmentMagicFlights)
    {
        if (auto* Actor = Pair.Key.Get()) Actor->OnFlightFactsChanged().Remove(Pair.Value.FactsHandle);
        if (Pair.Value.FlightRequestId.IsValid())
        {
            FGamePlatformPresentationRequest Cancel;
            Cancel.RequestId = Pair.Value.FlightRequestId; Cancel.SemanticTag = MagicSemantic(MagicName(Pair.Value.StyleId), false);
            Cancel.PredictionState = EGamePlatformPresentationPredictionState::Cancelled; SubmitProjectRequest(Cancel);
        }
    }
    DevelopmentMagicFlights.Reset();
    if (DevelopmentMagicPack.IsValid()) DeactivateContentPack(DevelopmentMagicPack);
    DevelopmentMagicPack = {}; DevelopmentMagicWorld.Reset();
}
