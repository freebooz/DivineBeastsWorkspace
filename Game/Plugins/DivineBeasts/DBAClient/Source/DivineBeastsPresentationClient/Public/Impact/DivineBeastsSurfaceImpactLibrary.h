// 项目层已确认环境命中到平台贴花的显式映射，不修改伤害或服务器碰撞。
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Marks/GamePlatformSurfaceMarkTypes.h"
#include "DivineBeastsSurfaceImpactLibrary.generated.h"

/**
 * 神兽联盟项目外观适配：不使用未经批准的技能身份，不把角色受击转成地形贴花。
 * MOBA事实接入者必须传已确认命中事件及真实世界表面，不能由纯视觉结果决定服务器命中。
 */
UCLASS()
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsSurfaceImpactLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 可用的中立语义：Impact.Projectile/Explosion/Slash/Energy/Fracture；未知值拒绝。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Presentation|Surface")
    static bool ResolveImpactVisualKind(FName ImpactSemantic, EGamePlatformSurfaceMarkKind& OutKind);

    /** 只对WorldStatic或WorldDynamic的真实阻挡命中生成；Pawn、空组件、未确认事件都拒绝。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Presentation|Surface", meta=(WorldContext="WorldContextObject"))
    static FGamePlatformSurfaceMarkResult SubmitConfirmedEnvironmentImpact(
        const UObject* WorldContextObject, const FHitResult& ConfirmedHit,
        FName ImpactSemantic, FVector AttackDirection, FGuid ConfirmedEventId, float SizeScale = 1.0f);
};
