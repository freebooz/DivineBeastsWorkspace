#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GamePlatformSurfaceMaterialAuthoringLibrary.generated.h"
class UMaterial;
class UMaterialExpression;

/** 仅编辑器内部的跨游戏材质作者接口。UE5.8隐藏Python Displacement枚举，原生连接由此提供。 */
UCLASS()
class GAMEPLATFORMSURFACEEDITOR_API UGamePlatformSurfaceMaterialAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 游戏线程：连接真实Displacement输入，不启用Nanite、不保存资产、不修改世界。失败返回false。 */
    UFUNCTION(BlueprintCallable, Category="Surface|Authoring")
    static bool ConnectDisplacement(UMaterialExpression* Expression);
};
