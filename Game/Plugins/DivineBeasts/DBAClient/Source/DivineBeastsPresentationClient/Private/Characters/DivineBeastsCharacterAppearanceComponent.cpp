// 神兽联盟项目层客户端外观适配：读取已复制英雄身份，通过既有兼容加载器取得Profile/视觉资源并装配Mesh。
// 本组件持有加载句柄、就绪委托及有限重试定时器；身份切换/EndPlay取消旧代请求，不参与服务器权威规则。
#include "Characters/DivineBeastsCharacterAppearanceComponent.h"

#include "Animation/AnimInstance.h" // TSoftClassPtr::Get需完整UAnimInstance类型，NoPCH下不依赖间接包含。
#include "Characters/DivineBeastsCharacterAppearanceCatalog.h"
#include "Characters/DivineBeastsCharacterAppearanceProfile.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h" // 软引用Get执行类型检查，需要网格资产完整类型而非组件头的前向声明。
#include "Engine/StreamableManager.h"
#include "GameFramework/Character.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
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
    DevelopmentDynamicMaterials.Reset();
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

    // 身份切换时先释放旧占位MID引用；随后SetMaterial会用新Profile重新覆盖所有需要的槽位。
    DevelopmentDynamicMaterials.Reset();

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

    // 正式生成的生肖原型Profile带独立PrototypeTint材质覆盖；
    // 运行时瞬态回退不带材质覆盖，继续使用UE5 Mannequin原材质的Paint Tint参数。
    if (Profile->bDevelopmentPlaceholder)
    {
        const FName TintParameterName = Profile->MaterialOverrides.IsEmpty()
            ? FName(TEXT("Paint Tint"))
            : FName(TEXT("PrototypeTint"));
        DevelopmentDynamicMaterials.Reserve(MeshComponent->GetNumMaterials());
        for (int32 MaterialIndex = 0;
             MaterialIndex < MeshComponent->GetNumMaterials();
             ++MaterialIndex)
        {
            UMaterialInterface* BaseMaterial = MeshComponent->GetMaterial(MaterialIndex);
            if (!BaseMaterial)
            {
                continue;
            }

            UMaterialInstanceDynamic* DynamicMaterial =
                UMaterialInstanceDynamic::Create(BaseMaterial, this);
            if (!DynamicMaterial)
            {
                continue;
            }

            DynamicMaterial->SetVectorParameterValue(
                TintParameterName,
                Profile->DevelopmentTint);
            MeshComponent->SetMaterial(MaterialIndex, DynamicMaterial);
            DevelopmentDynamicMaterials.Add(DynamicMaterial);
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
