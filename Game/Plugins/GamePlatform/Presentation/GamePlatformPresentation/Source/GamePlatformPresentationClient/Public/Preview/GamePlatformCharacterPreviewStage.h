#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GamePlatformCharacterPreviewStage.generated.h"

class UAnimInstance;
class UCameraComponent;
class UMaterialInterface;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * AGamePlatformCharacterPreviewStage（平台三维角色预览舞台）。
 *
 * 跨游戏通用的纯客户端展示 Actor：
 * - 不复制、不参与 Gameplay、不创建 ASC/Combat/Inventory/AI；
 * - 只持有预览 Mesh、Camera 与基础旋转/缩放控制；
 * - 项目层负责把自己的 Appearance/Profile 解析为已经加载完成的视觉资源。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMPRESENTATIONCLIENT_API AGamePlatformCharacterPreviewStage final
    : public AActor
{
    GENERATED_BODY()

public:
    AGamePlatformCharacterPreviewStage();

    /** 应用已经加载完成的角色表现资源；不会触发同步资源加载。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Presentation|CharacterPreview")
    bool ApplyPreviewAppearance(
        USkeletalMesh* SkeletalMesh,
        const TArray<UMaterialInterface*>& MaterialOverrides,
        TSubclassOf<UAnimInstance> AnimInstanceClass,
        FVector MeshRelativeLocation,
        FRotator MeshRelativeRotation,
        FVector MeshRelativeScale);

    /** 清空当前预览主体，保留舞台与相机。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Presentation|CharacterPreview")
    void ClearPreviewAppearance();

    /** 设置预览主体绝对Yaw，单位度。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Presentation|CharacterPreview")
    void SetPreviewYaw(float YawDegrees);

    /** 增量旋转预览主体，单位度。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Presentation|CharacterPreview")
    void AddPreviewYaw(float DeltaYawDegrees);

    /** 设置镜头距离，单位厘米；内部限制到安全范围。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Presentation|CharacterPreview")
    void SetCameraDistance(float DistanceCentimeters);

    UFUNCTION(BlueprintPure, Category="GamePlatform|Presentation|CharacterPreview")
    USkeletalMeshComponent* GetPreviewMeshComponent() const { return PreviewMesh; }

    UFUNCTION(BlueprintPure, Category="GamePlatform|Presentation|CharacterPreview")
    UCameraComponent* GetPreviewCamera() const { return PreviewCamera; }

private:
    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<USceneComponent> SubjectPivot;

    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<USkeletalMeshComponent> PreviewMesh;

    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<USceneComponent> CameraPivot;

    UPROPERTY(VisibleAnywhere, Category="Preview")
    TObjectPtr<UCameraComponent> PreviewCamera;

    float CameraDistanceCentimeters = 320.0f;
};
