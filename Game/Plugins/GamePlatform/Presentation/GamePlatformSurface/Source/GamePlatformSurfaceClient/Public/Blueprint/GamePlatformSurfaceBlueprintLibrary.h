#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/GamePlatformSurfaceTypes.h"
#include "GamePlatformSurfaceBlueprintLibrary.generated.h"

/**
 * 表面环境状态的蓝图入口。
 *
 * 蓝图仅提交表现状态；天气权威、世界规则和网络复制继续由上层系统拥有。
 */
UCLASS()
class GAMEPLATFORMSURFACECLIENT_API UGamePlatformSurfaceBlueprintLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 将环境表面状态应用到当前客户端世界。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Surface", meta=(WorldContext="WorldContextObject"))
    static FGamePlatformSurfaceUpdateResult ApplyEnvironmentState(
        const UObject* WorldContextObject,
        const FGamePlatformSurfaceEnvironmentState& State);

    /** 读取当前客户端世界已接受的表面状态和修订号。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Surface", meta=(WorldContext="WorldContextObject"))
    static bool GetEnvironmentState(
        const UObject* WorldContextObject,
        FGamePlatformSurfaceEnvironmentState& OutState,
        int32& OutRevision);

    /** 显式重新绑定MPC，并把当前状态重新推送到材质。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Surface", meta=(WorldContext="WorldContextObject"))
    static FGamePlatformSurfaceUpdateResult RefreshMaterialBinding(const UObject* WorldContextObject);
};
