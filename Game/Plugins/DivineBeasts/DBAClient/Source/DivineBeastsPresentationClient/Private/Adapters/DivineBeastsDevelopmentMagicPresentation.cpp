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
    const FGuid BindingToken = FGuid::NewGuid(); DevelopmentMagicBindingToken = BindingToken;
    DevelopmentMagicSpawnHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleDevelopmentMagicSpawn));
    DevelopmentMagicPackHandle = ContentPackChanged.AddUObject(this, &ThisClass::HandleDevelopmentMagicPack);
    FDivineBeastsPresentationContentPackFragment Pack;
    Pack.ContentPackId = TEXT("DBA.Development.Magic"); Pack.Revision = 1;
    Pack.LifecycleScope = EGamePlatformPresentationContextScope::World; Pack.bRequiredPreload = true;
    auto& Catalog = Pack.CatalogFragment;
    Catalog.FragmentId = Pack.ContentPackId; Catalog.OwnerScopeId = Pack.ContentPackId;
    Catalog.Revision = 1; Catalog.Scope = EGamePlatformPresentationCatalogScope::ContentPack; Catalog.LifecycleScope = Pack.LifecycleScope;
    for (const TCHAR* Name : {TEXT("Fireball"), TEXT("Frostbolt"), TEXT("ArcaneMissiles")})
        for (const TCHAR* Stage : {TEXT("Flight"),TEXT("Impact"),TEXT("Cast")})
        {
            FGamePlatformPresentationCatalogEntry Entry;
            Entry.EntryId = FName(*(FString(TEXT("DBA.Development.Magic.")) + Name + TEXT(".") + Stage));
            Entry.ProviderChannel = TEXT("VFX");
            // 目录身份使用DBA短名；已注册语义保留DivineBeasts稳定身份，不能由EntryId拼出不存在的标签。
            Entry.SemanticTag = FGameplayTag::RequestGameplayTag(FName(*(FString(TEXT("Presentation.DivineBeasts.Development.Magic."))+Name+TEXT(".")+Stage)),false);
            Entry.DefinitionId = FName(*(FString(TEXT("presentation.dba.development.magic.")) + FString(Name).ToLower() +
                TEXT(".") + FString(Stage).ToLower() + TEXT("@1")));
            Entry.ContextQuery.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
            Entry.Scope = Catalog.Scope; Entry.Specificity = 1; Entry.ContentRevision = TEXT("1");
            Catalog.Entries.Add(Entry);
        }
    FString Error;
    const auto Handle = ActivateContentPack(Pack, Error, &DevelopmentMagicPack);
    if (DevelopmentMagicBindingToken != BindingToken || DevelopmentMagicWorld.Get() != World)
    { if (Handle.IsValid()) DeactivateContentPack(Handle); return; }
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
    Projectile->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleDevelopmentMagicEndPlay);
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
    const FGuid Token = DevelopmentMagicBindingToken;
    for (auto& Weak : Actors)
    {
        if (Token != DevelopmentMagicBindingToken) return;
        if (auto* Actor = Weak.Get()) HandleDevelopmentMagicFacts(Actor, Actor->GetFlightFacts());
    }
}

void UDivineBeastsPresentationClientSubsystem::HandleDevelopmentMagicEndPlay(AActor* Ended, EEndPlayReason::Type)
{
    auto* Actor = Cast<AGamePlatformCombatProjectile>(Ended);
    FDevelopmentMagicFlight Old;
    if (!Actor || !DevelopmentMagicFlights.RemoveAndCopyValue(Actor, Old)) return;
    Actor->OnFlightFactsChanged().Remove(Old.FactsHandle);
    Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleDevelopmentMagicEndPlay);
    // 外调前已经移除旧账本；同步取消通知即使清理世界，也不能访问失效TMap指针。
    if (Old.FlightRequestId.IsValid())
    {
        FGamePlatformPresentationRequest Cancel; Cancel.RequestId = Old.FlightRequestId;
        Cancel.SemanticTag = MagicSemantic(MagicName(Old.StyleId),false);
        Cancel.PredictionState = EGamePlatformPresentationPredictionState::Cancelled;
        SubmitProjectRequest(Cancel);
    }
}

void UDivineBeastsPresentationClientSubsystem::HandleDevelopmentMagicFacts(
    AGamePlatformCombatProjectile* Actor, const FGamePlatformCombatProjectileFacts& Facts)
{
    // 复制来源可能在同步回调中改变，先保存值快照；任何TMap指针仅在外调之前借用。
    const FGamePlatformCombatProjectileFacts Snapshot = Facts;
    const FGuid Token = DevelopmentMagicBindingToken;
    auto* Flight = DevelopmentMagicFlights.Find(Actor);
    if (bClosing || !IsValid(Actor) || !Flight || Actor->GetWorld() != DevelopmentMagicWorld.Get() ||
        !Snapshot.bStarted || !Snapshot.OccurrenceId.IsValid() || Snapshot.SourceAvatarGeneration <= 0) return;
    if (Actor->IsActorBeingDestroyed()) { HandleDevelopmentMagicEndPlay(Actor,EEndPlayReason::Destroyed); return; }
    const FName Name = MagicName(Snapshot.PresentationStyleId);
    if (Name.IsNone()) return;
    Flight->StyleId = Snapshot.PresentationStyleId;
    if (Snapshot.bFinished)
    {
        const FGuid OldRequest = Flight->FlightRequestId;
        Flight->FlightRequestId.Invalidate(); Flight->bFlightEnded = true;
        if (OldRequest.IsValid())
        {
            FGamePlatformPresentationRequest Cancel; Cancel.RequestId=OldRequest;
            Cancel.SemanticTag=MagicSemantic(Name,false); Cancel.PredictionState=EGamePlatformPresentationPredictionState::Cancelled;
            SubmitProjectRequest(Cancel);
        }
    }
    if (DevelopmentMagicBindingToken != Token || !IsValid(Actor)) return;
    Flight = DevelopmentMagicFlights.Find(Actor); if (!Flight) return;
    FString Error;
    if (GetContentPackState(DevelopmentMagicPack, Error) != EDivineBeastsPresentationContentPackState::Active) return;
    const auto Submit = [this, Actor, &Snapshot, Name, Token](bool bImpact)
    {
        auto* Current = DevelopmentMagicFlights.Find(Actor); if (!Current || Token != DevelopmentMagicBindingToken) return;
        if (bImpact ? Current->bImpactSubmitted : Current->bFlightEnded || Current->FlightRequestId.IsValid()) return;
        FGamePlatformPresentationRequest Request;
        Request.RequestId = FGuid::NewGuid(); Request.SemanticTag = MagicSemantic(Name, bImpact);
        Request.ContextId = Snapshot.PresentationStyleId; Request.SourceId = Snapshot.AbilityId;
        Request.SourceLocation = bImpact ? Snapshot.ImpactLocation : Actor->GetActorLocation();
        Request.ImpactLocation = Snapshot.ImpactLocation; Request.ImpactNormal = Snapshot.ImpactNormal;
        Request.Context.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
        Request.Context.AvatarGeneration = Snapshot.SourceAvatarGeneration;
        Request.RequestGeneration = Snapshot.SourceAvatarGeneration;
        Request.WorldGeneration = PlatformPresentation ? PlatformPresentation->GetWorldGeneration() : 0;
        Request.Lifetime = bImpact ? EGamePlatformPresentationLifetime::Timed : EGamePlatformPresentationLifetime::Persistent;
        if (!bImpact) Request.AttachComponent = Actor->GetRootComponent();
        // 先发布处理中身份，避免provider/观察者同步重入同事实时再次受理。
        if (bImpact) Current->bImpactSubmitted=true; else Current->FlightRequestId=Request.RequestId;
        const auto Result=SubmitProjectRequest(Request);
        Current=DevelopmentMagicFlights.Find(Actor);
        if (Token != DevelopmentMagicBindingToken || !Current)
        {
            if (Result==EGamePlatformPresentationSubmitResult::Submitted)
            { Request.PredictionState=EGamePlatformPresentationPredictionState::Cancelled; SubmitProjectRequest(Request); }
            return;
        }
        if (Result!=EGamePlatformPresentationSubmitResult::Submitted)
        {
            if (bImpact) Current->bImpactSubmitted=false;
            else if (Current->FlightRequestId==Request.RequestId) Current->FlightRequestId.Invalidate();
        }
    };
    if (!Snapshot.bFinished) Submit(false);
    else if (Snapshot.bImpacted) Submit(true);
}

void UDivineBeastsPresentationClientSubsystem::UnbindDevelopmentMagicWorld()
{
    if (UWorld* World = DevelopmentMagicWorld.Get()) World->RemoveOnActorSpawnedHandler(DevelopmentMagicSpawnHandle);
    DevelopmentMagicSpawnHandle.Reset();
    ContentPackChanged.Remove(DevelopmentMagicPackHandle); DevelopmentMagicPackHandle.Reset();
    // 先整体撤销所有权，再跨同步取消/包终态通知。迭代的是本栈旧快照，不会被重入扩容/Reset破坏。
    auto OldFlights = MoveTemp(DevelopmentMagicFlights); DevelopmentMagicFlights.Reset();
    const auto OldPack=DevelopmentMagicPack; DevelopmentMagicPack={}; DevelopmentMagicWorld.Reset();
    DevelopmentMagicBindingToken=FGuid::NewGuid();
    for (auto& Pair : OldFlights)
    {
        if (auto* Actor = Pair.Key.Get())
        { Actor->OnFlightFactsChanged().Remove(Pair.Value.FactsHandle); Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleDevelopmentMagicEndPlay); }
        if (Pair.Value.FlightRequestId.IsValid())
        {
            FGamePlatformPresentationRequest Cancel;
            Cancel.RequestId = Pair.Value.FlightRequestId; Cancel.SemanticTag = MagicSemantic(MagicName(Pair.Value.StyleId), false);
            Cancel.PredictionState = EGamePlatformPresentationPredictionState::Cancelled; SubmitProjectRequest(Cancel);
        }
    }
    if (OldPack.IsValid()) DeactivateContentPack(OldPack);
}
