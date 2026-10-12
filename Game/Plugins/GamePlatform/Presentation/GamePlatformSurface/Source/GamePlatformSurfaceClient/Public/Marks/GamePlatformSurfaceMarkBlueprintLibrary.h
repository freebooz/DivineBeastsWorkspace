// 游戏／蓝图仅提交客户端命中事实，不直接控制贴花组件的生命周期。
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Marks/GamePlatformSurfaceMarkTypes.h"
#include "GamePlatformSurfaceMarkBlueprintLibrary.generated.h"

/** SurfaceMark统一蓝图桥接；默认不开启网络复制或物理破坏。 */
UCLASS()
class GAMEPLATFORMSURFACECLIENT_API UGamePlatformSurfaceMarkBlueprintLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Surface|Marks", meta=(WorldContext="WorldContextObject"))
    static FGamePlatformSurfaceMarkResult SubmitSurfaceMark(
        const UObject* WorldContextObject, const FGamePlatformSurfaceMarkRequest& Request);

    /** 从命中结果自动读取物理材质和组件，不调整Gameplay伤害事实。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Surface|Marks", meta=(WorldContext="WorldContextObject"))
    static FGamePlatformSurfaceMarkResult SubmitSurfaceMarkFromHit(
        const UObject* WorldContextObject, EGamePlatformSurfaceMarkKind Kind,
        const FHitResult& Hit, FVector AttackDirection, FGuid EventId, float SizeScale = 1.0f);

    UFUNCTION(BlueprintCallable, Category="GamePlatform|Surface|Marks", meta=(WorldContext="WorldContextObject"))
    static bool RefreshSurfaceMarkCatalog(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category="GamePlatform|Surface|Marks", meta=(WorldContext="WorldContextObject"))
    static bool GetSurfaceMarkCounts(const UObject* WorldContextObject, int32& Active, int32& Pending);
};
