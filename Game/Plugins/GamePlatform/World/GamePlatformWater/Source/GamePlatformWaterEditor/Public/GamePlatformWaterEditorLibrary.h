#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GamePlatformWaterEditorLibrary.generated.h"

class AGamePlatformWaterRegion;
class APhysicsVolume;
class UMaterialInterface;

/** 编辑器中复用引擎的Actor替换和引用重映射，创建不雕刻地形的原生湖体。 */
UCLASS()
class GAMEPLATFORMWATEREDITOR_API UGamePlatformWaterEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 仅处理编辑器世界中已有的禁雕刻湖体；成功返回保留原GUID的原生湖体。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Water|Editor")
    static AActor* ReplaceWithNativeNoCarveLake(AActor* Source, FString& OutError);

    /** GT编辑器：从禁雕刻闭合湖岸生成独立PhysicsVolume并绑定现有Region。
     * DepthCm为水面以下深度，范围100~20000cm；失败不绑定旧Box，不保存地图。
     * 不覆盖已有体积，湖体和Region必须同属一个Editor World。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Water|Editor")
    static APhysicsVolume* BakeLakePhysicsVolume(AActor* Source, AGamePlatformWaterRegion* Region,
        float DepthCm, FString& OutError);

    /** 只配置原生水下查询碰撞与材质；Pawn/Camera均不阻挡，不改变湖岸或雕刻设置。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Water|Editor")
    static bool ConfigureLakeUnderwater(AActor* Source, UMaterialInterface* Material, FString& OutError);
};
