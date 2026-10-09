/**
 * 项目层本地玩家角色预览：接收界面ViewModel命令，组合平台预览舞台和项目外观软引用。
 * 仅拥有本地相机、流送工作室及异步资源租约；不改变持久档案、世界角色或服务器动画权威。
 * 切换英雄、失活及世界退出依请求代次取消加载，解绑并释放本实例资源。
 */
#include "Characters/DivineBeastsCharacterPreviewSubsystem.h"

#include "Animation/AnimInstance.h" // 本文件也调用动画软类Get，完整类型不能由另一个Unity源文件提供。
#include "Characters/DivineBeastsCharacterAppearanceCatalog.h"
#include "Characters/DivineBeastsCharacterAppearanceProfile.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h" // 材质读取/设置及组件UObject转换需要完整类型，不能依赖PCH或Unity包含顺序。
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h" // 本文件读取Profile软网格引用，资产类型也必须直接完整包含。
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DefaultPawn.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Preview/GamePlatformCharacterPreviewStage.h"

// 只记录项目资源身份和生命周期代次，便于定位预览失败；不记录用户账号、密码或票据。
DEFINE_LOG_CATEGORY_STATIC(LogDivineBeastsCharacterPreview, Log, All);

namespace
{
    // 开发原型的待机只归本地预览；不写入通用角色外观Profile，不覆盖游戏内移动／权威动画。
    const TSoftClassPtr<UAnimInstance> DevelopmentPreviewIdleClass(
        FSoftObjectPath(TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/ABP_DBA_PreviewIdle.ABP_DBA_PreviewIdle_C")));
    const TSoftObjectPtr<UWorld> CharacterStudioWorld(
        FSoftObjectPath(
            TEXT("/DBAFrontEndPack/Maps/L_DBA_CharacterStudio.L_DBA_CharacterStudio")));
}

void UDivineBeastsCharacterPreviewSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this,
        &UDivineBeastsCharacterPreviewSubsystem::HandleWorldCleanup);
}

void UDivineBeastsCharacterPreviewSubsystem::Deinitialize()
{
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }

    DeactivatePreviewScene();
    Super::Deinitialize();
}

bool UDivineBeastsCharacterPreviewSubsystem::ActivatePreviewScene()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }

    ResolvePreviewStage();
    if (PreviewStage.IsValid())
    {
        BindPreviewCamera();
        TryApplyPendingAppearance();
        return true;
    }

    if (PreviewStreamingLevel)
    {
        return true;
    }

    bool bSuccess = false;
    PreviewStreamingLevel =
        ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
            this,
            CharacterStudioWorld,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            bSuccess);
    if (!bSuccess || !PreviewStreamingLevel)
    {
        PreviewStreamingLevel = nullptr;
        return false;
    }

    PreviewStreamingLevel->OnLevelShown.AddDynamic(
        this,
        &UDivineBeastsCharacterPreviewSubsystem::HandlePreviewLevelShown);
    return true;
}

void UDivineBeastsCharacterPreviewSubsystem::DeactivatePreviewScene()
{
    CancelPendingLoads();
    RequestedHeroDefinitionId = NAME_None;
    PendingProfile = nullptr;
    DevelopmentDynamicMaterials.Reset();

    // 默认观测球体只是本地前端宿主，不是权威角色。只恢复本次借用的实例，
    // 不隐藏其他玩家或正式ACharacter，不改变碰撞、控制及网络复制。
    if (ADefaultPawn* Observer = HiddenPreviewObserver.Get())
    {
        Observer->SetActorHiddenInGame(bObserverWasHidden);
    }
    HiddenPreviewObserver.Reset();

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    APlayerController* Controller =
        LocalPlayer ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
    if (Controller && PreviewStage.IsValid() &&
        Controller->GetViewTarget() == PreviewStage.Get() &&
        PreviousViewTarget.IsValid())
    {
        Controller->SetViewTarget(PreviousViewTarget.Get());
    }
    PreviousViewTarget.Reset();

    if (PreviewStage.IsValid())
    {
        PreviewStage->ClearPreviewAppearance();
    }
    PreviewStage.Reset();

    if (PreviewStreamingLevel)
    {
        PreviewStreamingLevel->OnLevelShown.RemoveDynamic(
            this,
            &UDivineBeastsCharacterPreviewSubsystem::HandlePreviewLevelShown);
        PreviewStreamingLevel->SetShouldBeVisible(false);
        PreviewStreamingLevel->SetShouldBeLoaded(false);
        PreviewStreamingLevel->SetIsRequestingUnloadAndRemoval(true);
        PreviewStreamingLevel = nullptr;
    }
}

bool UDivineBeastsCharacterPreviewSubsystem::PreviewHero(
    FName HeroDefinitionId)
{
    if (HeroDefinitionId.IsNone() || !ActivatePreviewScene())
    {
        return false;
    }

    if (RequestedHeroDefinitionId == HeroDefinitionId &&
        (PendingProfile || PreviewStage.IsValid()))
    {
        return true;
    }

    CancelPendingLoads();
    PendingProfile = nullptr;
    DevelopmentDynamicMaterials.Reset();
    RequestedHeroDefinitionId = HeroDefinitionId;

    const int32 ExpectedGeneration = ++RequestGeneration;
    const TWeakObjectPtr<UDivineBeastsCharacterPreviewSubsystem> WeakThis(this);
    ProfileLease =
        FDivineBeastsCharacterAppearanceCatalog::RequestDefaultProfile(
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
    return true;
}

void UDivineBeastsCharacterPreviewSubsystem::ClearPreview()
{
    CancelPendingLoads();
    RequestedHeroDefinitionId = NAME_None;
    PendingProfile = nullptr;
    DevelopmentDynamicMaterials.Reset();
    if (PreviewStage.IsValid())
    {
        PreviewStage->ClearPreviewAppearance();
    }
}

void UDivineBeastsCharacterPreviewSubsystem::RotatePreview(
    float DeltaYawDegrees)
{
    if (PreviewStage.IsValid())
    {
        PreviewStage->AddPreviewYaw(DeltaYawDegrees);
    }
}

void UDivineBeastsCharacterPreviewSubsystem::SetPreviewCameraDistance(
    float DistanceCentimeters)
{
    if (PreviewStage.IsValid())
    {
        PreviewStage->SetCameraDistance(DistanceCentimeters);
    }
}

void UDivineBeastsCharacterPreviewSubsystem::HandlePreviewLevelShown()
{
    ResolvePreviewStage();
    BindPreviewCamera();
    TryApplyPendingAppearance();
}

void UDivineBeastsCharacterPreviewSubsystem::HandleWorldCleanup(
    UWorld* World,
    bool,
    bool)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (World && LocalPlayer && LocalPlayer->GetWorld() == World)
    {
        CancelPendingLoads();
        PreviewStreamingLevel = nullptr;
        PreviewStage.Reset();
        PreviousViewTarget.Reset();
        // 世界销毁时原观测Pawn随世界退出，不向新世界实例恢复旧可见性。
        HiddenPreviewObserver.Reset();
        PendingProfile = nullptr;
        DevelopmentDynamicMaterials.Reset();
    }
}

void UDivineBeastsCharacterPreviewSubsystem::ResolvePreviewStage()
{
    if (PreviewStage.IsValid())
    {
        return;
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    if (!World)
    {
        return;
    }

    // 流送卸载是异步的：刚退出的关卡Actor仍可能出现在世界迭代器里。
    // 只接受当前实例已显示的关卡或显式打开的持久工作室；拒绝旧卸载关卡，
    // 避免退出后立即重进绑定一个即将销毁的舞台，并隔离其他本地玩家实例。
    ULevel* OwnedLevel = PreviewStreamingLevel && PreviewStreamingLevel->IsLevelVisible()
        ? PreviewStreamingLevel->GetLoadedLevel() : nullptr;
    for (TActorIterator<AGamePlatformCharacterPreviewStage> It(World); It; ++It)
    {
        if (It->GetLevel() != World->PersistentLevel && It->GetLevel() != OwnedLevel)
        {
            continue;
        }
        PreviewStage = *It;
        break;
    }
}

void UDivineBeastsCharacterPreviewSubsystem::BindPreviewCamera()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    APlayerController* Controller =
        LocalPlayer && World ? LocalPlayer->GetPlayerController(World) : nullptr;
    if (!Controller || !PreviewStage.IsValid())
    {
        return;
    }

    if (!PreviousViewTarget.IsValid())
    {
        PreviousViewTarget = Controller->GetViewTarget();
    }
    if (!HiddenPreviewObserver.IsValid())
    {
        if (ADefaultPawn* Observer = Cast<ADefaultPawn>(Controller->GetPawn());
            Observer && Observer->IsLocallyControlled())
        {
            bObserverWasHidden = Observer->IsHidden();
            HiddenPreviewObserver = Observer;
            Observer->SetActorHiddenInGame(true);
        }
    }
    Controller->SetViewTarget(PreviewStage.Get());
}

void UDivineBeastsCharacterPreviewSubsystem::CancelPendingLoads()
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

void UDivineBeastsCharacterPreviewSubsystem::HandleProfileLoaded(
    UDivineBeastsCharacterAppearanceProfile* Profile,
    FName ExpectedHeroDefinitionId,
    int32 ExpectedRequestGeneration)
{
    UE_LOG(LogDivineBeastsCharacterPreview, Display, TEXT("Profile callback: Hero=%s Generation=%d/%d Profile=%s"),
        *ExpectedHeroDefinitionId.ToString(), ExpectedRequestGeneration, RequestGeneration, *GetNameSafe(Profile));
    if (ExpectedRequestGeneration != RequestGeneration ||
        ExpectedHeroDefinitionId != RequestedHeroDefinitionId)
    {
        return;
    }

    ProfileLease.Reset();
    if (!Profile || Profile->HeroDefinitionId != ExpectedHeroDefinitionId)
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("Preview profile missing or hero identity mismatch."));
        return;
    }

    FString Error;
    if (!Profile->IsProfileValid(Error))
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("Invalid preview profile: %s"), *Error);
        return;
    }

    PendingProfile = Profile;

    TArray<FSoftObjectPath> Assets;
    if (!Profile->SkeletalMesh.IsNull())
    {
        Assets.Add(Profile->SkeletalMesh.ToSoftObjectPath());
    }
    for (const TSoftObjectPtr<UMaterialInterface>& Material :
         Profile->MaterialOverrides)
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
    else if (Profile->bDevelopmentPlaceholder)
    {
        // 使用同一异步资源租约，取消或切换英雄时仍由既有代次栅栏丢弃迟到回调。
        Assets.AddUnique(DevelopmentPreviewIdleClass.ToSoftObjectPath());
    }

    const TWeakObjectPtr<UDivineBeastsCharacterPreviewSubsystem> WeakThis(this);
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

    // 全部资源已常驻时加载器可能同步完成，统一尝试一次幂等应用。
    TryApplyPendingAppearance();
}

void UDivineBeastsCharacterPreviewSubsystem::HandleVisualResourcesLoaded(
    UDivineBeastsCharacterAppearanceProfile* Profile,
    FName ExpectedHeroDefinitionId,
    int32 ExpectedRequestGeneration)
{
    if (ExpectedRequestGeneration != RequestGeneration ||
        ExpectedHeroDefinitionId != RequestedHeroDefinitionId ||
        PendingProfile != Profile)
    {
        return;
    }

    VisualLease.Reset();
    TryApplyPendingAppearance();
}

bool UDivineBeastsCharacterPreviewSubsystem::TryApplyPendingAppearance()
{
    if (!PreviewStage.IsValid() || !PendingProfile ||
        PendingProfile->HeroDefinitionId != RequestedHeroDefinitionId)
    {
        return false;
    }

    USkeletalMesh* Mesh = PendingProfile->SkeletalMesh.Get();
    if (!Mesh)
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Verbose, TEXT("Preview mesh is not loaded: %s"), *PendingProfile->SkeletalMesh.ToSoftObjectPath().ToString());
        return false;
    }

    TArray<UMaterialInterface*> Materials;
    Materials.Reserve(PendingProfile->MaterialOverrides.Num());
    for (const TSoftObjectPtr<UMaterialInterface>& Material :
         PendingProfile->MaterialOverrides)
    {
        UMaterialInterface* Resolved = Material.Get();
        if (!Material.IsNull() && !Resolved)
        {
            return false;
        }
        Materials.Add(Resolved);
    }

    UClass* AnimClass = PendingProfile->AnimInstanceClass.IsNull()
        ? (PendingProfile->bDevelopmentPlaceholder ? DevelopmentPreviewIdleClass.Get() : nullptr)
        : PendingProfile->AnimInstanceClass.Get();
    if ((!PendingProfile->AnimInstanceClass.IsNull() || PendingProfile->bDevelopmentPlaceholder) && !AnimClass)
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("Required preview animation class is not loaded: Hero=%s"), *RequestedHeroDefinitionId.ToString());
        return false;
    }

    FString SkeletonError;
    if (!UDivineBeastsCharacterAppearanceProfile::ValidateLoadedSkeleton(Mesh, Mesh->GetSkeleton(), SkeletonError))
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("Preview mesh skeleton rejected: %s"), *SkeletonError);
        return false;
    }
    if (AnimClass)
    {
        // 显式动画和开发Idle采用相同门禁；原生AnimInstance没有蓝图目标骨架，由其自身动画合同负责。
        const IAnimClassInterface* AnimationInterface = IAnimClassInterface::GetFromClass(AnimClass);
        const USkeleton* AnimationSkeleton = AnimationInterface ? AnimationInterface->GetTargetSkeleton() : nullptr;
        if (AnimationInterface && !UDivineBeastsCharacterAppearanceProfile::ValidateLoadedSkeleton(Mesh, AnimationSkeleton, SkeletonError))
        {
            UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("Preview animation skeleton rejected: Hero=%s AnimClass=%s Reason=%s"),
                *RequestedHeroDefinitionId.ToString(), *GetNameSafe(AnimClass), *SkeletonError);
            return false;
        }
    }

    if (!PreviewStage->ApplyPreviewAppearance(
            Mesh,
            Materials,
            TSubclassOf<UAnimInstance>(AnimClass),
            PendingProfile->MeshRelativeLocation,
            PendingProfile->MeshRelativeRotation,
            PendingProfile->MeshRelativeScale))
    {
        UE_LOG(LogDivineBeastsCharacterPreview, Warning, TEXT("Preview stage rejected appearance: Hero=%s"), *RequestedHeroDefinitionId.ToString());
        return false;
    }

    UE_LOG(LogDivineBeastsCharacterPreview, Display, TEXT("Preview appearance applied: Hero=%s Mesh=%s AnimClass=%s"),
        *RequestedHeroDefinitionId.ToString(), *GetNameSafe(Mesh), *GetNameSafe(AnimClass));

    DevelopmentDynamicMaterials.Reset();
    if (PendingProfile->bDevelopmentPlaceholder)
    {
        USkeletalMeshComponent* MeshComponent =
            PreviewStage->GetPreviewMeshComponent();
        const FName TintParameterName =
            PendingProfile->MaterialOverrides.IsEmpty()
                ? FName(TEXT("Paint Tint"))
                : FName(TEXT("PrototypeTint"));
        if (MeshComponent)
        {
            DevelopmentDynamicMaterials.Reserve(
                MeshComponent->GetNumMaterials());
            for (int32 Index = 0;
                 Index < MeshComponent->GetNumMaterials();
                 ++Index)
            {
                UMaterialInterface* BaseMaterial =
                    MeshComponent->GetMaterial(Index);
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
                    PendingProfile->DevelopmentTint);
                MeshComponent->SetMaterial(Index, DynamicMaterial);
                DevelopmentDynamicMaterials.Add(DynamicMaterial);
            }
        }
    }

    return true;
}
