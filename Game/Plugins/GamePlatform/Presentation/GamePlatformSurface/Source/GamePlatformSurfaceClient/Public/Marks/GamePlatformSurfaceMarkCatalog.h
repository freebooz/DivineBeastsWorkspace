// 通用客户端贴花目录，材质全部软引用，不附带任何生肖或项目身份。
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "Marks/GamePlatformSurfaceMarkTypes.h"
#include "GamePlatformSurfaceMarkCatalog.generated.h"

class UMaterialInterface;

/** 表面类型专用配置；SurfaceType_Default为同一痕迹类别的回退配方。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSURFACECLIENT_API FGamePlatformSurfaceMarkStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SurfaceMark")
    EGamePlatformSurfaceMarkKind Kind = EGamePlatformSurfaceMarkKind::Bullet;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SurfaceMark")
    TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SurfaceMark")
    TSoftObjectPtr<UMaterialInterface> Material;
    /** 贴花尺寸X是投射深度，Y/Z是宽高；厘米。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SurfaceMark", meta=(Units="cm"))
    FVector SizeCm = FVector(12.0, 24.0, 24.0);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SurfaceMark", meta=(ClampMin="0.0", Units="s"))
    float LifetimeSeconds = 40.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SurfaceMark", meta=(ClampMin="0.0", Units="s"))
    float FadeSeconds = 6.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SurfaceMark", meta=(Units="cm"))
    float NormalOffsetCm = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SurfaceMark")
    bool bAttachToHitComponent = true;
    bool IsValidStyle() const;
};

/** 可在不同游戏中复用的数据资产；不承担同步、碰撞或真实破坏。 */
UCLASS(BlueprintType)
class GAMEPLATFORMSURFACECLIENT_API UGamePlatformSurfaceMarkCatalog final : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SurfaceMark")
    TArray<FGamePlatformSurfaceMarkStyle> Styles;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Budget", meta=(ClampMin="1", ClampMax="512"))
    int32 MaxActiveMarks = 128;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Budget", meta=(ClampMin="1", ClampMax="128"))
    int32 MaxPendingMarks = 32;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Budget", meta=(ClampMin="1", ClampMax="2048"))
    int32 MaxRecentEventIds = 256;

    const FGamePlatformSurfaceMarkStyle* FindStyle(
        EGamePlatformSurfaceMarkKind Kind, EPhysicalSurface SurfaceType) const;
    bool IsValidCatalog(FString& OutError) const;
};
