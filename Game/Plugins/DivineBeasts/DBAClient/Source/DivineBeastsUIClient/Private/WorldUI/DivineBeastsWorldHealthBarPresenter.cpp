// 项目头顶血条：玩家消费ASC生命事件，开发怪物消费唯一复制LifeState；不发送请求或改生命。
#include "WorldUI/DivineBeastsWorldHealthBarPresenter.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Characters/DivineBeastsDevelopmentMonster.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Components/SceneComponent.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Services/GamePlatformWorldUIService.h"
#include "WorldUI/GamePlatformWorldWidgetBase.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

void UDivineBeastsWorldHealthBarPresenter::Initialize(ULocalPlayer* InPlayer, UWorld* InWorld,
    TSubclassOf<UGamePlatformWorldWidgetBase> InWidgetClass)
{
    if (ObservedWorld.Get() == InWorld && LocalPlayer.Get() == InPlayer && WidgetClass == InWidgetClass)
    {
        auto* Controller = InPlayer ? InPlayer->GetPlayerController(InWorld) : nullptr;
        AActor* NewPawn = Controller ? Controller->GetPawn() : nullptr;
        AActor* OldPawn = ExcludedLocalPawn.Get(); ExcludedLocalPawn = NewPawn;
        RemoveActor(NewPawn);
        if (OldPawn && OldPawn != NewPawn && !OldPawn->IsActorBeingDestroyed()) ObserveActor(OldPawn);
        return;
    }
    Clear();
    if (!InPlayer || !InWorld || InWorld->bIsTearingDown || !InWidgetClass || InWorld->GetNetMode() == NM_DedicatedServer) return;
    auto* Manager = InPlayer->GetSubsystem<UGamePlatformUIManagerSubsystem>();
    if (!Manager || !Manager->GetWorldUIService()) return;
    LocalPlayer = InPlayer; ObservedWorld = InWorld; WidgetClass = InWidgetClass; Service = Manager->GetWorldUIService();
    if (auto* Controller = InPlayer->GetPlayerController(InWorld)) ExcludedLocalPawn = Controller->GetPawn();
    const uint64 Generation = ObservationGeneration;
    RootReadyHandle = Service->OnRootLayoutChanged().AddWeakLambda(this, [this, Generation]()
    {
        if (Generation != ObservationGeneration || !ObservedWorld.IsValid() || ObservedWorld->bIsTearingDown || !Service.IsValid()) return;
        // 只重试事件缓存，不扫描世界或重新读取生命；初始Construct早于服务就绪也可正确显示。
        TArray<TWeakObjectPtr<AActor>> Actors; Bindings.GetKeys(Actors);
        for (const auto& Actor : Actors) if (Actor.IsValid() && Generation == ObservationGeneration) Publish(Actor.Get());
    });
    SpawnHandle = InWorld->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateWeakLambda(this,
        [this, Generation](AActor* Actor) { if (Generation == ObservationGeneration) ObserveActor(Actor); }));
    CleanupHandle = FWorldDelegates::OnWorldCleanup.AddWeakLambda(this,
        [this, Generation](UWorld* World, bool, bool)
        { if (Generation == ObservationGeneration && ObservedWorld.Get() == World) Clear(); });
    // 仅进入世界的一次性冷启动盘点；新实体和生命变化以后均来自通知，不执行定时扫描。
    for (TActorIterator<ADivineBeastsCharacter> It(InWorld); It; ++It) ObserveActor(*It);
}

void UDivineBeastsWorldHealthBarPresenter::ObserveActor(AActor* Actor)
{
    if (!IsValid(Actor) || !Actor->IsA<ADivineBeastsCharacter>() || Actor->GetWorld() != ObservedWorld.Get() ||
        Bindings.Contains(Actor) || Bindings.Num() >= 128) return;
    auto* Controller = LocalPlayer.IsValid() ? LocalPlayer->GetPlayerController(ObservedWorld.Get()) : nullptr;
    if (Controller && Controller->GetPawn() == Actor) return; // 自己已有主HUD血条。
    auto& Binding = Bindings.Add(Actor);
    Binding.Request.RequestId = FGuid::NewGuid();
    Binding.Request.SurfaceKey = TEXT("DBA.World.HealthBar"); Binding.Request.StyleId = TEXT("DBA.World.HealthBar");
    Binding.Request.MaxVisibleDistance = 6000.f; Binding.Request.Resource.ResourceId = TEXT("Health");
    Binding.Request.PrimaryText = FText::FromString(Actor->IsA<ADivineBeastsDevelopmentMonster>() ? TEXT("训练怪物") : TEXT("玩家"));
    Actor->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleActorDestroyed);
    Actor->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleActorEndPlay);
    const uint64 Generation = ObservationGeneration;
    const TWeakObjectPtr<AActor> WeakActor(Actor);
    if (auto* Root = Actor->GetRootComponent())
    {
        Binding.TransformSource = Root;
        Binding.TransformHandle = Root->TransformUpdated.AddWeakLambda(this,
            [this, Generation, WeakActor](USceneComponent*, EUpdateTransformFlags, ETeleportType)
            { if (Generation == ObservationGeneration && WeakActor.IsValid()) Publish(WeakActor.Get()); });
    }
    if (auto* Monster = Cast<ADivineBeastsDevelopmentMonster>(Actor))
    {
        Binding.MonsterLifeHandle = Monster->OnLifeStateChanged().AddWeakLambda(this,
            [this, Generation, WeakActor](const FDivineBeastsDevelopmentMonsterLifeState& State)
            { if (Generation == ObservationGeneration && WeakActor.IsValid()) ApplyHealth(WeakActor.Get(), State.Health, State.MaxHealth); });
        const auto State = Monster->GetLifeState(); ApplyHealth(Actor, State.Health, State.MaxHealth);
    }
    else if (auto* ASC = Actor->FindComponentByClass<UGamePlatformAbilitySystemComponent>())
    {
        Binding.ASC = ASC;
        auto Changed = [this, Generation, WeakActor](const FOnAttributeChangeData&)
            { if (Generation == ObservationGeneration && WeakActor.IsValid()) ReadPlayerHealth(WeakActor.Get()); };
        Binding.HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetHealthAttribute()).AddWeakLambda(this, Changed);
        Binding.MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetMaxHealthAttribute()).AddWeakLambda(this, Changed);
        ReadPlayerHealth(Actor);
    }
}

void UDivineBeastsWorldHealthBarPresenter::ReadPlayerHealth(AActor* Actor)
{
    const auto* Binding = Bindings.Find(Actor);
    auto* ASC = Binding ? Binding->ASC.Get() : nullptr;
    if (const auto* Attributes = ASC ? ASC->GetSet<UGamePlatformCombatAttributeSet>() : nullptr)
        ApplyHealth(Actor, Attributes->GetHealth(), Attributes->GetMaxHealth());
}

void UDivineBeastsWorldHealthBarPresenter::ApplyHealth(AActor* Actor, float Health, float Maximum)
{
    auto* Binding = Bindings.Find(Actor); if (!Binding) return;
    // 缺少合法复制值时保持隐藏，不把0/0伪装为满血；实际比例由平台统一裁剪。
    Binding->Request.Resource.CurrentValue = FMath::IsFinite(Health) ? Health : 0.f;
    Binding->Request.Resource.MaximumValue = FMath::IsFinite(Maximum) ? Maximum : 0.f;
    Binding->Request.SecondaryText = Maximum > 0.f && FMath::IsFinite(Health) && FMath::IsFinite(Maximum)
        ? FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), FMath::Max(Health, 0.f), Maximum)) : FText::GetEmpty();
    Publish(Actor);
}

void UDivineBeastsWorldHealthBarPresenter::Publish(AActor* Actor)
{
    auto* Binding = Bindings.Find(Actor); auto* UI = Service.Get();
    if (!Binding || !UI || !IsValid(Actor)) return;
    FVector Origin, Extent; Actor->GetActorBounds(true, Origin, Extent);
    Binding->Request.WorldLocation = Origin + FVector(0, 0, Extent.Z + 24.f);
    Binding->Request.bDisplayRequested = bVisible && Binding->Request.Resource.MaximumValue > 0.f;
    // 重建来源在Travel之后由新代次负责；注册失败时保留缓存，后续真实事件可重试。
    const auto Request = Binding->Request;
    if (!UI->UpdateWorldUI(Request.RequestId, Request)) UI->RegisterWorldUI(Request, WidgetClass);
}

void UDivineBeastsWorldHealthBarPresenter::RefreshProjection()
{
    if (auto* UI = Service.Get()) UI->RefreshProjection();
}

void UDivineBeastsWorldHealthBarPresenter::SetVisible(bool bInVisible)
{
    if (bVisible == bInVisible) return;
    bVisible = bInVisible;
    TArray<TWeakObjectPtr<AActor>> Actors; Bindings.GetKeys(Actors);
    for (const auto& Actor : Actors) if (Actor.IsValid()) Publish(Actor.Get());
}

void UDivineBeastsWorldHealthBarPresenter::HandleActorDestroyed(AActor* Actor) { RemoveActor(Actor); }
void UDivineBeastsWorldHealthBarPresenter::HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type) { RemoveActor(Actor); }

void UDivineBeastsWorldHealthBarPresenter::RemoveActor(AActor* Actor)
{
    FDivineBeastsWorldHealthBarBinding Binding;
    if (!Bindings.RemoveAndCopyValue(Actor, Binding)) return;
    if (auto* Monster = Cast<ADivineBeastsDevelopmentMonster>(Actor)) Monster->OnLifeStateChanged().Remove(Binding.MonsterLifeHandle);
    if (auto* ASC = Binding.ASC.Get())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetHealthAttribute()).Remove(Binding.HealthHandle);
        ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetMaxHealthAttribute()).Remove(Binding.MaxHealthHandle);
    }
    if (auto* Root = Binding.TransformSource.Get()) Root->TransformUpdated.Remove(Binding.TransformHandle);
    if (Actor) Actor->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleActorDestroyed);
    if (Actor) Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleActorEndPlay);
    if (auto* UI = Service.Get()) UI->UnregisterWorldUI(Binding.Request.RequestId);
}

void UDivineBeastsWorldHealthBarPresenter::Clear()
{
    ++ObservationGeneration;
    if (auto* UI = Service.Get()) UI->OnRootLayoutChanged().Remove(RootReadyHandle);
    RootReadyHandle.Reset();
    if (auto* World = ObservedWorld.Get()) World->RemoveOnActorSpawnedHandler(SpawnHandle);
    FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle); SpawnHandle.Reset(); CleanupHandle.Reset();
    TArray<TWeakObjectPtr<AActor>> Actors; Bindings.GetKeys(Actors);
    for (const auto& Actor : Actors) if (AActor* Object = Actor.GetEvenIfUnreachable()) RemoveActor(Object);
    Bindings.Empty(); Service.Reset(); ObservedWorld.Reset(); LocalPlayer.Reset(); ExcludedLocalPawn.Reset(); WidgetClass = nullptr;
}

void UDivineBeastsWorldHealthBarPresenter::BeginDestroy() { Clear(); Super::BeginDestroy(); }
