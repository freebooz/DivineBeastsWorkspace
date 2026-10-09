// 平台客户端三维角色预览舞台：由上层UI传入已加载外观资源，不承担网络权威或资源加载。
// Actor拥有预览网格/相机组件；调用方持有资源租约，清空或世界销毁结束展示。
#include "Preview/GamePlatformCharacterPreviewStage.h"

#include "Animation/AnimInstance.h" // TSubclassOf转UClass需完整UAnimInstance，不能依赖PCH/Unity偶然可见。
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace
{
    constexpr float MinimumPreviewCameraDistance = 150.0f;
    constexpr float MaximumPreviewCameraDistance = 800.0f;
}

AGamePlatformCharacterPreviewStage::AGamePlatformCharacterPreviewStage()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetReplicateMovement(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    SubjectPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SubjectPivot"));
    SubjectPivot->SetupAttachment(SceneRoot);

    PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
    PreviewMesh->SetupAttachment(SubjectPivot);
    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewMesh->SetGenerateOverlapEvents(false);
    PreviewMesh->SetCanEverAffectNavigation(false);

    CameraPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraPivot"));
    CameraPivot->SetupAttachment(SceneRoot);
    CameraPivot->SetRelativeLocation(FVector(0.0, 0.0, 95.0));

    PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
    PreviewCamera->SetupAttachment(CameraPivot);
    PreviewCamera->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
    PreviewCamera->bConstrainAspectRatio = false;
    SetCameraDistance(CameraDistanceCentimeters);
}

bool AGamePlatformCharacterPreviewStage::ApplyPreviewAppearance(
    USkeletalMesh* SkeletalMesh,
    const TArray<UMaterialInterface*>& MaterialOverrides,
    TSubclassOf<UAnimInstance> AnimInstanceClass,
    FVector MeshRelativeLocation,
    FRotator MeshRelativeRotation,
    FVector MeshRelativeScale)
{
    if (!PreviewMesh || !SkeletalMesh ||
        MeshRelativeLocation.ContainsNaN() ||
        MeshRelativeRotation.ContainsNaN() ||
        MeshRelativeScale.ContainsNaN() ||
        MeshRelativeScale.GetMin() <= 0.0)
    {
        return false;
    }

    PreviewMesh->SetSkeletalMesh(SkeletalMesh, true);
    PreviewMesh->SetRelativeLocation(MeshRelativeLocation);
    PreviewMesh->SetRelativeRotation(MeshRelativeRotation);
    PreviewMesh->SetRelativeScale3D(MeshRelativeScale);

    if (MaterialOverrides.Num() == 1 && MaterialOverrides[0])
    {
        for (int32 Index = 0; Index < PreviewMesh->GetNumMaterials(); ++Index)
        {
            PreviewMesh->SetMaterial(Index, MaterialOverrides[0]);
        }
    }
    else
    {
        const int32 Count = FMath::Min(
            MaterialOverrides.Num(),
            PreviewMesh->GetNumMaterials());
        for (int32 Index = 0; Index < Count; ++Index)
        {
            if (MaterialOverrides[Index])
            {
                PreviewMesh->SetMaterial(Index, MaterialOverrides[Index]);
            }
        }
    }

    PreviewMesh->SetAnimInstanceClass(AnimInstanceClass);
    // 外观偏移通常以胶囊中心为原点，而舞台以脚底为原点。
    // 仅在本地组件按真实网格包围盒归零高度，避免复用胶囊偏移截腿；
    // 不修改外观资产、权威碰撞或角色Actor世界坐标。
    const FBox PreviewBounds = PreviewMesh->CalcBounds(PreviewMesh->GetRelativeTransform()).GetBox();
    FVector GroundedLocation = PreviewMesh->GetRelativeLocation();
    GroundedLocation.Z -= PreviewBounds.Min.Z;
    PreviewMesh->SetRelativeLocation(GroundedLocation);
    return true;
}

void AGamePlatformCharacterPreviewStage::ClearPreviewAppearance()
{
    if (!PreviewMesh)
    {
        return;
    }

    PreviewMesh->SetAnimInstanceClass(nullptr);
    PreviewMesh->SetSkeletalMesh(nullptr);
}

void AGamePlatformCharacterPreviewStage::SetPreviewYaw(float YawDegrees)
{
    if (!SubjectPivot || !FMath::IsFinite(YawDegrees))
    {
        return;
    }

    SubjectPivot->SetRelativeRotation(
        FRotator(0.0, FRotator::ClampAxis(YawDegrees), 0.0));
}

void AGamePlatformCharacterPreviewStage::AddPreviewYaw(float DeltaYawDegrees)
{
    if (!SubjectPivot || !FMath::IsFinite(DeltaYawDegrees))
    {
        return;
    }

    SetPreviewYaw(
        SubjectPivot->GetRelativeRotation().Yaw + DeltaYawDegrees);
}

void AGamePlatformCharacterPreviewStage::SetCameraDistance(
    float DistanceCentimeters)
{
    if (!PreviewCamera || !FMath::IsFinite(DistanceCentimeters))
    {
        return;
    }

    CameraDistanceCentimeters = FMath::Clamp(
        DistanceCentimeters,
        MinimumPreviewCameraDistance,
        MaximumPreviewCameraDistance);
    PreviewCamera->SetRelativeLocation(
        FVector(CameraDistanceCentimeters, 0.0, 0.0));
}
