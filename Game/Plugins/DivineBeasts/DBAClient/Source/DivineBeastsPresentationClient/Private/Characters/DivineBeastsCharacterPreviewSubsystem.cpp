// 神兽联盟项目层LocalPlayer预览适配：加载预览场景并把已选外观交给平台预览舞台，不参与服务器权威。
// 本服务持有场景、外观加载句柄和动态材质；失活/世界清理/退出撤销旧代加载并恢复预览相机所有权。
#include "Characters/DivineBeastsCharacterPreviewSubsystem.h"

#include "Animation/AnimInstance.h" // 本文件也调用动画软类Get，完整类型不能由另一个Unity源文件提供。
#include "Characters/DivineBeastsCharacterAppearanceCatalog.h"
#include "Characters/DivineBeastsCharacterAppearanceProfile.h"
#include "Components/SkeletalMeshComponent.h" // 材质读取/设置及组件UObject转换需要完整类型，不能依赖PCH或Unity包含顺序。
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h" // 本文件读取Profile软网格引用，资产类型也必须直接完整包含。
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Preview/GamePlatformCharacterPreviewStage.h"

namespace
{
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

    for (TActorIterator<AGamePlatformCharacterPreviewStage> It(World); It; ++It)
    {
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
    if (ExpectedRequestGeneration != RequestGeneration ||
        ExpectedHeroDefinitionId != RequestedHeroDefinitionId)
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
        ? nullptr
        : PendingProfile->AnimInstanceClass.Get();
    if (!PendingProfile->AnimInstanceClass.IsNull() && !AnimClass)
    {
        return false;
    }

    if (!PreviewStage->ApplyPreviewAppearance(
            Mesh,
            Materials,
            TSubclassOf<UAnimInstance>(AnimClass),
            PendingProfile->MeshRelativeLocation,
            PendingProfile->MeshRelativeRotation,
            PendingProfile->MeshRelativeScale))
    {
        return false;
    }

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
