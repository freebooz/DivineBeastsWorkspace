#include "Characters/DivineBeastsCharacterAppearanceComponent.h"

#include "Characters/DivineBeastsCharacterAppearanceCatalog.h"
#include "Characters/DivineBeastsCharacterAppearanceProfile.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Character.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

UDivineBeastsCharacterAppearanceComponent::UDivineBeastsCharacterAppearanceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

void UDivineBeastsCharacterAppearanceComponent::BeginPlay()
{
    Super::BeginPlay();
    TryBindCharacterState();
}

void UDivineBeastsCharacterAppearanceComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (CharacterState && ReadinessDelegateHandle.IsValid())
    {
        CharacterState->OnReadinessChanged().Remove(ReadinessDelegateHandle);
    }
    ReadinessDelegateHandle.Reset();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(StateRetryTimer);
    }

    CancelPendingLoads();
    CharacterState = nullptr;
    PendingProfile = nullptr;
    Super::EndPlay(EndPlayReason);
}

void UDivineBeastsCharacterAppearanceComponent::TryBindCharacterState()
{
    if (CharacterState)
    {
        return;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    CharacterState = Owner->FindComponentByClass<UDivineBeastsCharacterComponent>();
    if (CharacterState)
    {
        ReadinessDelegateHandle = CharacterState->OnReadinessChanged().AddUObject(
            this,
            &UDivineBeastsCharacterAppearanceComponent::HandleCharacterReadinessChanged);
        RefreshAppearance();
        return;
    }

    // 动态复制组件通常在Actor初始复制阶段即可到达；这里最多重试约5秒，
    // 避免因极端网络时序漏掉绑定，同时不为每个角色开启永久Tick。
    if (RemainingStateBindRetries > 0)
    {
        --RemainingStateBindRetries;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                StateRetryTimer,
                this,
                &UDivineBeastsCharacterAppearanceComponent::TryBindCharacterState,
                0.25f,
                false);
        }
    }
}

void UDivineBeastsCharacterAppearanceComponent::HandleCharacterReadinessChanged(bool)
{
    RefreshAppearance();
}

void UDivineBeastsCharacterAppearanceComponent::RefreshAppearance()
{
    if (!CharacterState)
    {
        return;
    }

    const FName HeroDefinitionId = CharacterState->GetHeroDefinitionId();
    if (HeroDefinitionId.IsNone())
    {
        return;
    }

    if (AppliedHeroDefinitionId == HeroDefinitionId && PendingProfile)
    {
        return;
    }

    CancelPendingLoads();
    PendingProfile = nullptr;

    const int32 ExpectedGeneration = ++RequestGeneration;
    const TWeakObjectPtr<UDivineBeastsCharacterAppearanceComponent> WeakThis(this);

    ProfileLease = FDivineBeastsCharacterAppearanceCatalog::RequestDefaultProfile(
        HeroDefinitionId,
        [WeakThis, HeroDefinitionId, ExpectedGeneration](
            UDivineBeastsCharacterAppearanceProfile* Profile)
        {
            if (WeakThis.IsValid())
            {
                WeakThis->HandleProfileLoaded(
                    Profile,
                    HeroDefinitionId,
                    ExpectedGeneration);
            }
        });
}

void UDivineBeastsCharacterAppearanceComponent::HandleProfileLoaded(
    UDivineBeastsCharacterAppearanceProfile* Profile,
    FName ExpectedHeroDefinitionId,
    int32 ExpectedRequestGeneration)
{
    if (ExpectedRequestGeneration != RequestGeneration ||
        !CharacterState ||
        CharacterState->GetHeroDefinitionId() != ExpectedHeroDefinitionId)
    {
        return;
    }

    ProfileLease.Reset();
    if (!Profile || Profile->HeroDefinitionId != ExpectedHeroDefinitionId)
    {
        return;
    }

    FString Error;
    if (!Profile->IsProfileValid(Error))
    {
        return;
    }

    PendingProfile = Profile;

    TArray<FSoftObjectPath> Assets;
    Assets.Reserve(2 + Profile->MaterialOverrides.Num());
    Assets.Add(Profile->SkeletalMesh.ToSoftObjectPath());

    for (const TSoftObjectPtr<UMaterialInterface>& Material : Profile->MaterialOverrides)
    {
        if (!Material.IsNull())
        {
            Assets.AddUnique(Material.ToSoftObjectPath());
        }
    }

    if (!Profile->AnimInstanceClass.IsNull())
    {
        Assets.AddUnique(Profile->AnimInstanceClass.ToSoftObjectPath());
    }

    const TWeakObjectPtr<UDivineBeastsCharacterAppearanceComponent> WeakThis(this);
    VisualLease = FGamePlatformAssetLoader::RequestAsyncLoad(
        Assets,
        FStreamableDelegate::CreateLambda(
            [WeakThis, Profile, ExpectedHeroDefinitionId, ExpectedRequestGeneration]()
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->HandleVisualResourcesLoaded(
                        Profile,
                        ExpectedHeroDefinitionId,
                        ExpectedRequestGeneration);
                }
            }));

    // 所有资源已经常驻时统一加载器可能不需要保留有效Handle，
    // 完成委托仍会负责最终装配；这里不做同步Load，避免产生第二条加载路径。
}

void UDivineBeastsCharacterAppearanceComponent::HandleVisualResourcesLoaded(
    UDivineBeastsCharacterAppearanceProfile* Profile,
    FName ExpectedHeroDefinitionId,
    int32 ExpectedRequestGeneration)
{
    if (ExpectedRequestGeneration != RequestGeneration ||
        !CharacterState ||
        CharacterState->GetHeroDefinitionId() != ExpectedHeroDefinitionId ||
        PendingProfile != Profile)
    {
        return;
    }

    VisualLease.Reset();

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    USkeletalMeshComponent* MeshComponent = Character ? Character->GetMesh() : nullptr;
    USkeletalMesh* SkeletalMesh = Profile ? Profile->SkeletalMesh.Get() : nullptr;
    if (!MeshComponent || !SkeletalMesh)
    {
        return;
    }

    MeshComponent->SetSkeletalMesh(SkeletalMesh, true);
    MeshComponent->SetRelativeLocation(Profile->MeshRelativeLocation);
    MeshComponent->SetRelativeRotation(Profile->MeshRelativeRotation);
    MeshComponent->SetRelativeScale3D(Profile->MeshRelativeScale);

    if (Profile->MaterialOverrides.Num() == 1)
    {
        UMaterialInterface* SharedOverride = Profile->MaterialOverrides[0].Get();
        if (SharedOverride)
        {
            for (int32 MaterialIndex = 0;
                 MaterialIndex < MeshComponent->GetNumMaterials();
                 ++MaterialIndex)
            {
                MeshComponent->SetMaterial(MaterialIndex, SharedOverride);
            }
        }
    }
    else
    {
        const int32 Count = FMath::Min(
            Profile->MaterialOverrides.Num(),
            MeshComponent->GetNumMaterials());
        for (int32 MaterialIndex = 0; MaterialIndex < Count; ++MaterialIndex)
        {
            if (UMaterialInterface* Override = Profile->MaterialOverrides[MaterialIndex].Get())
            {
                MeshComponent->SetMaterial(MaterialIndex, Override);
            }
        }
    }

    if (UClass* AnimClass = Profile->AnimInstanceClass.Get())
    {
        MeshComponent->SetAnimInstanceClass(AnimClass);
    }

    AppliedHeroDefinitionId = ExpectedHeroDefinitionId;
}

void UDivineBeastsCharacterAppearanceComponent::CancelPendingLoads()
{
    ++RequestGeneration;

    if (ProfileLease.IsValid())
    {
        FGamePlatformAssetLoader::Cancel(ProfileLease);
        ProfileLease.Reset();
    }

    if (VisualLease.IsValid())
    {
        FGamePlatformAssetLoader::Cancel(VisualLease);
        VisualLease.Reset();
    }
}
