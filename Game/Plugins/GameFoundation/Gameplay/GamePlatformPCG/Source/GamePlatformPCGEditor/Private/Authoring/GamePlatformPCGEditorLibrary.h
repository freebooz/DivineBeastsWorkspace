#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GamePlatformPCGEditorLibrary.generated.h"

class UGamePlatformPCGProfileDefinition;

/** Python/蓝图编辑器入口；固定开发资产范围，拒绝正式地图和已有包覆盖，不是运行时服务。 */
UCLASS()
class UGamePlatformPCGEditorLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 游戏线程，仅首次创建固定开发图/配置；任一目标已占用则整批拒绝，不覆盖用户资产。不执行图或地图操作。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Editor")
    static bool CreateDevelopmentAssets(FString& Error);
    /** 游戏线程；Profile及其图/网格必须已加载且保存。返回真实依赖字节指纹，不批准生成结果或重开状态。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|PCG|Editor")
    static bool InspectProfileSource(UGamePlatformPCGProfileDefinition* Profile, FString& Fingerprint,
        TArray<FString>& Dependencies, FString& Error);
};
