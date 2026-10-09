// 项目层客户端角色外观组件：由角色只读状态驱动骨骼、材质与动画资源的异步加载，不承载服务器权威规则。
// 依赖平台数据加载服务；加载请求、状态委托、重试计时器及动态材质由本组件持有。
// 生命周期跟随所属角色，EndPlay解绑状态、取消请求并释放自身引用，避免离开世界后应用过期结果。
#include "Characters/DivineBeastsCharacterAppearanceComponent.h"

// TSoftClassPtr::Get会调用UAnimInstance::StaticClass，必须包含完整类型，不能依赖Unity或共享PCH。
#include "Animation/AnimInstance.h"

#include "Characters/DivineBeastsCharacterAppearanceCatalog.h"
#include "Characters/DivineBeastsCharacterAppearanceProfile.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h" // 软引用Get执行类型检查，需要网格资产完整类型而非组件头的前向声明。
#include "Engine/StreamableManager.h"
#include "Engine/SkeletalMesh.h"
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
FName UDivineBeastsCharacterAppearanceComponent::CurrentVisualHero() const
{
    const FName Bound=CharacterState?CharacterState->GetHeroDefinitionId():NAME_None;
    return Bound.IsNone()?ApprovedVisualHero:Bound;
}
void UDivineBeastsCharacterAppearanceComponent::ApplyApprovedVisualHero(FName HeroId)
{
    if(GetNetMode()==NM_DedicatedServer || HeroId.IsNone())return;
    ApprovedVisualHero=HeroId; TryBindCharacterState(); RefreshAppearance();
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

    const FName HeroDefinitionId = CurrentVisualHero();
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
        CurrentVisualHero() != ExpectedHeroDefinitionId)
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
    else if(Profile->bDevelopmentPlaceholder)
    {
        // 一期真实占位模型沿用已交付IDLE；只加载客户端表现，不能作为服务器技能/根运动依据。
        Assets.AddUnique(FSoftObjectPath(TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/ABP_DBA_PreviewIdle.ABP_DBA_PreviewIdle_C")));
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
        CurrentVisualHero() != ExpectedHeroDefinitionId ||
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

    UClass* AnimClass = Profile->AnimInstanceClass.Get();
    if (Profile->AnimInstanceClass.IsNull() && Profile->bDevelopmentPlaceholder)
    {
        AnimClass = Cast<UClass>(FSoftObjectPath(TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/ABP_DBA_PreviewIdle.ABP_DBA_PreviewIdle_C")).ResolveObject());
    }
    if ((!Profile->AnimInstanceClass.IsNull() || Profile->bDevelopmentPlaceholder) && !AnimClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("角色外观动画尚未加载，拒绝本次装配：%s"), *ExpectedHeroDefinitionId.ToString());
        return;
    }
    FString SkeletonError;
    // 在改网格/材质之前检查，避免新身份挂上旧骨树；原有请求代次及取消门禁保持有效。
    UClass* EffectiveAnimClass = AnimClass ? AnimClass : MeshComponent->GetAnimClass();
    const bool bCompatible = UDivineBeastsCharacterAppearanceProfile::ValidateLoadedAnimationClass(SkeletalMesh, EffectiveAnimClass, SkeletonError);
    if (!bCompatible)
    {
        UE_LOG(LogTemp, Warning, TEXT("角色外观骨架不兼容，拒绝本次装配：Hero=%s Reason=%s"), *ExpectedHeroDefinitionId.ToString(), *SkeletonError);
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

    if (AnimClass)
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
