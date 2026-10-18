// SSLM头顶血条：玩家与正式新手村狼怪共享唯一GAS生命属性事件；纯客户端显示，不发送伤害请求。
#include "WorldUI/DivineBeastsWorldHealthBarPresenter.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Characters/DivineBeastsVillageWolf.h" // 狼怪是现有ADivineBeastsCharacter子类，复用相同ASC生命通知。
#include "Characters/DivineBeastsVillageVampire.h" // 人型怪物沿用同一生命通知，但显示独立名称。
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Services/GamePlatformWorldUIService.h"
#include "WorldUI/GamePlatformWorldWidgetBase.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"

void UDivineBeastsWorldHealthBarPresenter::Initialize(ULocalPlayer* InPlayer, UWorld* InWorld,
    TSubclassOf<UGamePlatformWorldWidgetBase> InWidgetClass, UMaterialInterface* InSelectionRingMaterial)
{
    if (ObservedWorld.Get() == InWorld && LocalPlayer.Get() == InPlayer && WidgetClass == InWidgetClass &&
        SelectionRingMaterial == InSelectionRingMaterial)
    {
        auto* Controller = InPlayer ? InPlayer->GetPlayerController(InWorld) : nullptr;
        AActor* NewPawn = Controller ? Controller->GetPawn() : nullptr;
        AActor* OldPawn = ExcludedLocalPawn.Get(); ExcludedLocalPawn = NewPawn;
        if (OldPawn != NewPawn) SetSelectedTarget(nullptr);
        RemoveActor(NewPawn);
        if (OldPawn && OldPawn != NewPawn && !OldPawn->IsActorBeingDestroyed()) ObserveActor(OldPawn);
        BindSelectionController(Cast<ADivineBeastsWorldPlayerController>(Controller));
        return;
    }
    Clear();
    if (!InPlayer || !InWorld || InWorld->bIsTearingDown || !InWidgetClass || InWorld->GetNetMode() == NM_DedicatedServer) return;
    auto* Manager = InPlayer->GetSubsystem<UGamePlatformUIManagerSubsystem>();
    if (!Manager || !Manager->GetWorldUIService()) return;
    LocalPlayer = InPlayer; ObservedWorld = InWorld; WidgetClass = InWidgetClass; Service = Manager->GetWorldUIService();
    SelectionRingMaterial = InSelectionRingMaterial;
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
    BindSelectionController(Cast<ADivineBeastsWorldPlayerController>(InPlayer->GetPlayerController(InWorld)));
}

void UDivineBeastsWorldHealthBarPresenter::ObserveActor(AActor* Actor, bool bSelectionOnly)
{
    UWorld* World = ObservedWorld.Get();
    if (!World || World->bIsTearingDown || !IsValid(Actor) ||
        (!bSelectionOnly && !Actor->IsA<ADivineBeastsCharacter>()) || Actor->GetWorld() != World ||
        Bindings.Contains(Actor) || Bindings.Num() >= 128) return;
    auto* Controller = LocalPlayer.IsValid() ? LocalPlayer->GetPlayerController(ObservedWorld.Get()) : nullptr;
    if (Controller && Controller->GetPawn() == Actor) return; // 自己已有主HUD血条。
    auto& Binding = Bindings.Add(Actor);
    Binding.bSelectionOnly = bSelectionOnly;
    Binding.Request.RequestId = FGuid::NewGuid();
    Binding.Request.SurfaceKey = TEXT("DBA.World.HealthBar"); Binding.Request.StyleId = TEXT("DBA.World.HealthBar");
    Binding.Request.MaxVisibleDistance = 6000.f; Binding.Request.Resource.ResourceId = TEXT("Health");
    Binding.Request.PrimaryText = FText::FromString(Actor->IsA<ADivineBeastsVillageVampire>() ? TEXT("吸血鬼") :
        Actor->IsA<ADivineBeastsVillageWolf>() ? TEXT("野狼") :
        Actor->IsA<ADivineBeastsCharacter>() ? TEXT("玩家") : TEXT("目标"));
    // 项目名称板已移除选中框/标记Widget；也不再向名称添加箭头或已选中文案。
    Binding.Request.SelectionText = FText::GetEmpty();
    Binding.Request.bCompactSelection = false;
    Actor->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleActorDestroyed);
    Actor->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleActorEndPlay);
    const uint64 Generation = ObservationGeneration;
    const TWeakObjectPtr<AActor> WeakActor(Actor);
    if (auto* Combat = Actor->FindComponentByClass<UGamePlatformCombatComponent>())
    {
        Binding.Combat = Combat;
        Binding.CombatObservationHandle = Combat->OnCombatObservationChanged().AddWeakLambda(this,
            [this, Generation, WeakActor]()
            { if (Generation == ObservationGeneration && WeakActor.IsValid()) Publish(WeakActor.Get()); });
    }
    if (auto* Identity = Actor->FindComponentByClass<UDivineBeastsCharacterComponent>())
    {
        Binding.Identity = Identity;
        Binding.IdentityHandle = Identity->OnReadinessChanged().AddWeakLambda(this,
            [this, Generation, WeakActor](bool)
            {
                if (Generation != ObservationGeneration || !WeakActor.IsValid()) return;
                BindPlayerState(WeakActor.Get()); Publish(WeakActor.Get());
            });
    }
    BindPlayerState(Actor);
    if (auto* Root = Actor->GetRootComponent())
    {
        Binding.TransformSource = Root;
        Binding.TransformHandle = Root->TransformUpdated.AddWeakLambda(this,
            [this, Generation, WeakActor](USceneComponent*, EUpdateTransformFlags, ETeleportType)
            { if (Generation == ObservationGeneration && WeakActor.IsValid()) Publish(WeakActor.Get()); });
    }
    // 已移除远古石灵独立LifeState；狼怪与玩家都从平台GAS属性读客户端复制真值。
    if (auto* ASC = Actor->FindComponentByClass<UGamePlatformAbilitySystemComponent>())
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
    if (SelectedTarget.Get() == Actor && (Binding->Request.Resource.CurrentValue <= 0.f || Maximum <= 0.f))
        SetSelectedTarget(nullptr);
    Publish(Actor);
}

void UDivineBeastsWorldHealthBarPresenter::Publish(AActor* Actor)
{
    // Travel销毁Pawn时仍可能同步触发控制器关系通知；旧World开始退出后禁止再创建UMG实例。
    UWorld* World = ObservedWorld.Get();
    if (!World || World->bIsTearingDown || !IsValid(Actor) || Actor->GetWorld() != World) return;
    auto* Binding = Bindings.Find(Actor);
    if (!Binding) return;
    if (SelectedTarget.Get() == Actor)
    {
        const auto* Combat = Actor->FindComponentByClass<UGamePlatformCombatComponent>();
        const auto* Player = Binding->PlayerState.Get();
        const auto PlayerSnapshot = Player ? Player->GetLifecycleSnapshot() : FGamePlatformPlayerLifecycleSnapshot();
        if ((Combat && (Combat->IsCombatDead() || Combat->GetCombatAvatarGeneration() != SelectedAvatarGeneration)) ||
            (!Combat && SelectedAvatarGeneration > 0) ||
            (PlayerSnapshot.IsRegistered() && PlayerSnapshot.ControlledPawn != Actor) ||
            PlayerSnapshot.Stage == EGamePlatformPlayerStage::Leaving || PlayerSnapshot.Stage == EGamePlatformPlayerStage::Removed ||
            PlayerSnapshot.Stage == EGamePlatformPlayerStage::Failed)
        {
            SetSelectedTarget(nullptr);
            return;
        }
    }
    Binding->Request.SelectionTint = ResolveSelectionTint(Actor);
    FVector Origin, Extent; Actor->GetActorBounds(true, Origin, Extent);
    Binding->Request.WorldLocation = Origin + FVector(0, 0, Extent.Z + 24.f);
    if (const auto* Character = Cast<ACharacter>(Actor))
    {
        // 导入外观的碰撞Bounds可能把名称板抬到远处目标血条上；胶囊是稳定锚点。
        // 无血条NPC只显示选中提示，放低半个胶囊高度，避开其后方怪物的血条。
        if (const auto* Capsule = Character->GetCapsuleComponent())
            Binding->Request.WorldLocation = Capsule->GetComponentLocation() +
                FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() *
                    (Binding->bSelectionOnly ? 0.5f : 1.0f) + 24.f);
    }
    Binding->Request.bDisplayRequested = (bVisible && Binding->Request.Resource.MaximumValue > 0.f) || Binding->Request.bSelected;
    if (SelectedTarget.Get() == Actor) RefreshSelectionRing();
    auto* UI = Service.Get(); if (!UI) return;
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

void UDivineBeastsWorldHealthBarPresenter::BindSelectionController(ADivineBeastsWorldPlayerController* Controller)
{
    if (SelectionController.Get() == Controller) return;
    if (auto* Previous = SelectionController.Get())
    {
        Previous->OnWorldTargetChanged.Remove(SelectionHandle);
        Previous->OnLocalWorldFactsChanged.Remove(WorldFactsHandle);
    }
    SelectionHandle.Reset(); WorldFactsHandle.Reset();
    const uint64 Generation = ObservationGeneration;
    SetSelectedTarget(nullptr);
    if (Generation != ObservationGeneration) return;
    SelectionController = Controller;
    if (!Controller || Controller->GetWorld() != ObservedWorld.Get()) return;
    const TWeakObjectPtr<ADivineBeastsWorldPlayerController> WeakController(Controller);
    SelectionHandle = Controller->OnWorldTargetChanged.AddWeakLambda(this, [this, Generation, WeakController]()
    {
        if (Generation == ObservationGeneration && WeakController.IsValid() && SelectionController == WeakController)
            RefreshSelectedTarget();
    });
    WorldFactsHandle = Controller->OnLocalWorldFactsChanged.AddWeakLambda(this, [this, Generation, WeakController]()
    {
        if (Generation == ObservationGeneration && WeakController.IsValid() && SelectionController == WeakController)
            RefreshRelations();
    });
    RefreshSelectedTarget();
}

void UDivineBeastsWorldHealthBarPresenter::RefreshSelectedTarget()
{
    auto* Controller = SelectionController.Get();
    SetSelectedTarget(Controller && Controller->GetWorld() == ObservedWorld.Get() ? Controller->GetSelectedWorldTarget() : nullptr);
}

void UDivineBeastsWorldHealthBarPresenter::SetSelectedTarget(AActor* Actor)
{
    const uint64 Generation = ObservationGeneration;
    const uint64 Revision = ++SelectionRevision;
    const TWeakObjectPtr<AActor> Previous = SelectedTarget;
    SelectedTarget.Reset(); SelectedAvatarGeneration = 0; ClearSelectionRing();
    bool bRemovePrevious = false;
    if (auto* Old = Bindings.Find(Previous)) { Old->Request.bSelected = false; bRemovePrevious = Old->bSelectionOnly; }
    if (Previous.IsValid())
    {
        if (bRemovePrevious) RemoveActor(Previous.Get()); else Publish(Previous.Get());
    }
    // UMG应用事件可同步拆除整个世界UI；旧选择栈不能在Clear之后重新持有目标。
    UWorld* World = ObservedWorld.Get();
    if (Generation != ObservationGeneration || Revision != SelectionRevision || !World || World->bIsTearingDown ||
        !IsValid(Actor) || Actor->GetWorld() != World) return;
    if (!Bindings.Contains(Actor)) ObserveActor(Actor, true);
    auto* Binding = Bindings.Find(Actor);
    auto* Combat = Actor->FindComponentByClass<UGamePlatformCombatComponent>();
    if (!Binding || (Combat && Combat->IsCombatDead()) ||
        (Binding->Request.Resource.MaximumValue > 0.f && Binding->Request.Resource.CurrentValue <= 0.f) || Actor->IsActorBeingDestroyed()) return;
    SelectedTarget = Actor; SelectedAvatarGeneration = Combat ? Combat->GetCombatAvatarGeneration() : 0;
    Binding->Request.bSelected = true;
    Publish(Actor);
}

void UDivineBeastsWorldHealthBarPresenter::BindPlayerState(AActor* Actor)
{
    auto* Binding = Bindings.Find(Actor); if (!Binding) return;
    auto* Pawn = Cast<APawn>(Actor);
    auto* State = Pawn ? Pawn->GetPlayerState<AGamePlatformPlayerStateBase>() : nullptr;
    if (Binding->PlayerState.Get() == State) return;
    if (auto* Previous = Binding->PlayerState.Get()) Previous->OnLifecycleChanged.Remove(Binding->PlayerStateHandle);
    Binding->PlayerState = State; Binding->PlayerStateHandle.Reset();
    if (!State) return;
    const uint64 Generation = ObservationGeneration;
    const TWeakObjectPtr<AActor> WeakActor(Actor);
    Binding->PlayerStateHandle = State->OnLifecycleChanged.AddWeakLambda(this, [this, Generation, WeakActor]()
    {
        if (Generation == ObservationGeneration && WeakActor.IsValid()) Publish(WeakActor.Get());
    });
}

void UDivineBeastsWorldHealthBarPresenter::RefreshRelations()
{
    // 只遍历本Presenter已订阅的有界快照；不扫描世界、不重新读生命，也不会复活死亡后的旧选择。
    const uint64 Generation = ObservationGeneration;
    TArray<TWeakObjectPtr<AActor>> Actors; Bindings.GetKeys(Actors);
    for (const auto& Actor : Actors)
    {
        if (Generation != ObservationGeneration) return;
        if (Actor.IsValid()) { BindPlayerState(Actor.Get()); Publish(Actor.Get()); }
    }
}

FLinearColor UDivineBeastsWorldHealthBarPresenter::ResolveSelectionTint(AActor* Actor) const
{
    if (Actor && Actor->IsA<ADivineBeastsVillageWolf>()) return FLinearColor(1.f, .12f, .08f);
    auto* World = ObservedWorld.Get();
    auto* State = World ? World->GetGameState() : nullptr;
    const auto* Experience = State ? State->FindComponentByClass<UGamePlatformExperienceComponent>() : nullptr;
    FName Role;
    const auto* Pawn = Cast<APawn>(Actor);
    const auto* Player = Pawn ? Pawn->GetPlayerState<AGamePlatformPlayerStateBase>() : nullptr;
    const auto* Controller = SelectionController.Get();
    const auto* OwnPlayer = Controller ? Controller->GetPlayerState<AGamePlatformPlayerStateBase>() : nullptr;
    const FGamePlatformId ExperienceId = Experience ? Experience->GetExperienceSnapshot().ExperienceId : FGamePlatformId();
    // 与Village服务器已批准的“玩家互为友方”一致：双方必须是当前公开准入Pawn。
    // ParticipantId是匿名准入身份，不能拿它或显示名冒充Party PlayerId；未知/竞技关系保留中立金色。
    if (ExperienceId.IsValid() && FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(
            FName(*(ExperienceId.Namespace + TEXT(".") + ExperienceId.Name)), Role) && Role == FDivineBeastsProjectCatalog::GetVillageServerRole() &&
        Player && OwnPlayer && Player->GetLifecycleSnapshot().IsRegistered() && OwnPlayer->GetLifecycleSnapshot().IsRegistered() &&
        Player->GetLifecycleSnapshot().ControlledPawn == Pawn && OwnPlayer->GetLifecycleSnapshot().ControlledPawn == Controller->GetPawn())
        return FLinearColor(.15f, 1.f, .38f);
    return FLinearColor(1.f, .73f, .18f);
}

void UDivineBeastsWorldHealthBarPresenter::RefreshSelectionRing()
{
    auto* Actor = SelectedTarget.Get(); auto* World = ObservedWorld.Get();
    const auto* Binding = Actor ? Bindings.Find(Actor) : nullptr;
    auto* Instance = World ? World->GetGameInstance() : nullptr;
    // 普通Decal没有LocalPlayer视口过滤能力；分屏时保留各自名称板，避免把一个玩家的选中圈显示给另一个。
    if (!Actor || !Binding || !SelectionRingMaterial || !World || World->bIsTearingDown ||
        World->GetNetMode() == NM_DedicatedServer || !Instance || Instance->GetLocalPlayers().Num() > 1)
    { ClearSelectionRing(); return; }
    if (!SelectionRing)
    {
        SelectionRing = NewObject<UDecalComponent>(this);
        SelectionRing->SetComponentTickEnabled(false);
        SelectionRing->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
        SelectionRing->SetFadeScreenSize(0.f);
        SelectionRingInstance = UMaterialInstanceDynamic::Create(SelectionRingMaterial, this);
        SelectionRing->SetDecalMaterial(SelectionRingInstance);
        SelectionRing->RegisterComponentWithWorld(World);
    }
    FVector Origin, Extent; Actor->GetActorBounds(true, Origin, Extent);
    // 远处人型目标的原始Bounds过窄；最低一米半径让脚下状态在常用选择距离仍可辨认。
    const float RadiusCm = FMath::Clamp(FMath::Max(Extent.X, Extent.Y) * 1.35f, 100.f, 220.f);
    const FVector BoundsFoot = Origin - FVector(0.f, 0.f, Extent.Z);
    FHitResult GroundHit;
    FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(DivineBeastsTargetRingGround), false, Actor);
    // 动画和导入模型可改变网格Bounds；用真实地面命中定位，不让法环埋入地形。
    const bool bGroundHit = World->LineTraceSingleByChannel(GroundHit,
        BoundsFoot + FVector(0.f, 0.f, 150.f), BoundsFoot - FVector(0.f, 0.f, 250.f),
        ECC_Visibility, GroundQuery);
    SelectionRing->DecalSize = FVector(64.f, RadiusCm, RadiusCm);
    SelectionRing->SetWorldLocation((bGroundHit ? GroundHit.ImpactPoint : BoundsFoot) + FVector(0.f, 0.f, 12.f));
    SelectionRingInstance->SetVectorParameterValue(TEXT("SelectionColor"), Binding->Request.SelectionTint);
    SelectionRing->MarkRenderStateDirty();
}

void UDivineBeastsWorldHealthBarPresenter::ClearSelectionRing()
{
    if (SelectionRing) SelectionRing->DestroyComponent();
    SelectionRing = nullptr; SelectionRingInstance = nullptr;
}

void UDivineBeastsWorldHealthBarPresenter::HandleActorDestroyed(AActor* Actor) { RemoveActor(Actor); }
void UDivineBeastsWorldHealthBarPresenter::HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type) { RemoveActor(Actor); }

void UDivineBeastsWorldHealthBarPresenter::RemoveActor(AActor* Actor)
{
    if (SelectedTarget.Get() == Actor) SetSelectedTarget(nullptr);
    FDivineBeastsWorldHealthBarBinding Binding;
    if (!Bindings.RemoveAndCopyValue(Actor, Binding)) return;
    if (auto* ASC = Binding.ASC.Get())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetHealthAttribute()).Remove(Binding.HealthHandle);
        ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetMaxHealthAttribute()).Remove(Binding.MaxHealthHandle);
    }
    if (auto* Root = Binding.TransformSource.Get()) Root->TransformUpdated.Remove(Binding.TransformHandle);
    if (auto* Identity = Binding.Identity.Get()) Identity->OnReadinessChanged().Remove(Binding.IdentityHandle);
    if (auto* State = Binding.PlayerState.Get()) State->OnLifecycleChanged.Remove(Binding.PlayerStateHandle);
    if (auto* Combat = Binding.Combat.Get()) Combat->OnCombatObservationChanged().Remove(Binding.CombatObservationHandle);
    if (Actor) Actor->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleActorDestroyed);
    if (Actor) Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleActorEndPlay);
    if (auto* UI = Service.Get()) UI->UnregisterWorldUI(Binding.Request.RequestId);
}

void UDivineBeastsWorldHealthBarPresenter::Clear()
{
    ++ObservationGeneration;
    ++SelectionRevision;
    if (auto* Controller = SelectionController.Get())
    {
        Controller->OnWorldTargetChanged.Remove(SelectionHandle);
        Controller->OnLocalWorldFactsChanged.Remove(WorldFactsHandle);
    }
    SelectionHandle.Reset(); WorldFactsHandle.Reset(); SelectionController.Reset();
    SelectedTarget.Reset(); SelectedAvatarGeneration = 0; ClearSelectionRing(); SelectionRingMaterial = nullptr;
    if (auto* UI = Service.Get()) UI->OnRootLayoutChanged().Remove(RootReadyHandle);
    RootReadyHandle.Reset();
    if (auto* World = ObservedWorld.Get()) World->RemoveOnActorSpawnedHandler(SpawnHandle);
    FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle); SpawnHandle.Reset(); CleanupHandle.Reset();
    TArray<TWeakObjectPtr<AActor>> Actors; Bindings.GetKeys(Actors);
    for (const auto& Actor : Actors) if (AActor* Object = Actor.GetEvenIfUnreachable()) RemoveActor(Object);
    Bindings.Empty(); Service.Reset(); ObservedWorld.Reset(); LocalPlayer.Reset(); ExcludedLocalPawn.Reset(); WidgetClass = nullptr;
}

void UDivineBeastsWorldHealthBarPresenter::BeginDestroy() { Clear(); Super::BeginDestroy(); }
