// 平台通用电影镜头配置：客户端数据资产，不包含项目角色、地图及网络权威逻辑。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GamePlatformCinematicCameraProfile.generated.h"

/** 镜头用途决定默认的视觉安全限制，不决定服务器玩法。 */
UENUM(BlueprintType)
enum class EGamePlatformCameraPresentationMode : uint8
{
    Exploration UMETA(DisplayName="探索"),
    CharacterPreview UMETA(DisplayName="角色展示"),
    Cinematic UMETA(DisplayName="剧情演出"),
    Arena UMETA(DisplayName="竞技")
};

/** 可在编辑器中编辑的电影镜头预设；数值使用毫米、f值、厘米和秒。 */
UCLASS(BlueprintType)
class GAMEPLATFORMCAMERACLIENT_API UGamePlatformCinematicCameraProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    /** 场景类型；竞技模式限制强景深及镜头干扰，实际玩法镜头仍由原系统控制。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera|Mode")
    EGamePlatformCameraPresentationMode Mode = EGamePlatformCameraPresentationMode::Exploration;

    /** 虚拟传感器宽度（毫米）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera|Optics", meta=(ClampMin="12.0", ClampMax="70.0"))
    float SensorWidthMm = 36.0f;

    /** 镜头焦距（毫米）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera|Optics", meta=(ClampMin="12.0", ClampMax="200.0"))
    float FocalLengthMm = 35.0f;

    /** 光圈f值。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera|Optics", meta=(ClampMin="1.2", ClampMax="22.0"))
    float Aperture = 5.6f;

    /** 手动焦点距离（厘米），仅非竞技演示模式可用。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera|Optics", meta=(ClampMin="10.0", ClampMax="100000.0"))
    float FocusDistanceCm = 300.0f;

    /** 切入和恢复时长（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera|Transition", meta=(ClampMin="0.0", ClampMax="5.0"))
    float BlendTimeSeconds = 0.65f;

    /** 允许非竞技镜头使用电影景深；禁用时保留当前相机已有光学状态。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera|Accessibility")
    bool bEnableCinematicFocus = true;

    /** 按毫米焦距与传感器宽度计算等效水平视场角；无效输入安全返回0。 */
    static float CalculateHorizontalFieldOfViewDegrees(float FocalLengthMm, float SensorWidthMm);

    /** 数值校验不加载资产，也不修改摄像机。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Camera")
    bool IsValidProfile() const;
};
