#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GamePlatformSurfaceMaterialAuthoringLibrary.generated.h"
class UMaterial;
class UMaterialExpression;
class UStaticMesh;
class UWorld;
class AActor;

/** 仅编辑器内部的跨游戏材质作者接口。UE5.8隐藏Python Displacement枚举，原生连接由此提供。 */
UCLASS()
class GAMEPLATFORMSURFACEEDITOR_API UGamePlatformSurfaceMaterialAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 游戏线程：连接真实Displacement输入，不启用Nanite、不保存资产、不修改世界。失败返回false。 */
    UFUNCTION(BlueprintCallable, Category="Surface|Authoring")
    static bool ConnectDisplacement(UMaterialExpression* Expression);
    /** 游戏线程只读列出母材质/材质函数的真实节点；不创建、保存或编译资产。其他类型返回空数组。 */
    UFUNCTION(BlueprintCallable, Category="Surface|Authoring")
    static TArray<UMaterialExpression*> GetMaterialExpressions(UObject* Asset);
    /** 只读LOD0所有UV通道的有限数/范围/非零数量，供源权重审计；返回JSON，不把通道存在当有效权重。 */
    UFUNCTION(BlueprintCallable, Category="Surface|Authoring")
    static FString GetStaticMeshUVStatistics(UStaticMesh* Mesh);
    /** 仅Editor World生成审核HISM夹具；变换为世界坐标，不修改原Actor/导航，不自动保存。失败返回nullptr。 */
    UFUNCTION(BlueprintCallable, Category="Surface|Authoring")
    static AActor* SpawnInstancedWindReviewActor(UWorld* World, UStaticMesh* Mesh, const TArray<FTransform>& WorldTransforms);
};
