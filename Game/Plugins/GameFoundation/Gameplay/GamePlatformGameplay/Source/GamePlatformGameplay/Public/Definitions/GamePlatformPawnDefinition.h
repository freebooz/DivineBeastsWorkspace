#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "GameFramework/Pawn.h"
#include "UObject/SoftObjectPtr.h"
#include "GamePlatformPawnDefinition.generated.h"

/**
 * 中立受控实体定义；只描述服务器批准的Pawn类、共享依赖和出生包络。
 * 不取代未来HeroDefinition，不包含生肖、技能、装备、阵营、数值成长或客户端任意选择。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMGAMEPLAY_API UGamePlatformPawnDefinition : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()

public:
    /**
     * 服务器可生成的APawn软类；必须由Data的GameplayShared分组预载，生成路径禁止同步加载。
     * 资产脚本只写项目批准类，不接受客户端、URL或后端直接传入任意类路径。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay", meta=(AssetBundles="GameplayShared"))
    TSoftClassPtr<APawn> PawnClass;

    /** 双端共享定义；每项必须同时位于RequiredDefinitions并随Pawn租约持有。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    TArray<FPrimaryAssetId> SharedDefinitions;

    /**
     * 出生占位包络半尺寸，单位厘米；各轴有限且大于零。
     * 最终碰撞仍由真实Pawn CDO和UWorld生成碰撞策略验证，不能以此宣称NavMesh有效。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay", meta=(ClampMin="0.001", Units="cm"))
    FVector SpawnEnvelopeHalfExtentCentimeters = FVector(42.f, 42.f, 96.f);

    /** 中立用途键，仅供策略和诊断，不代表正式角色类型。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FName PawnPurpose = TEXT("Default");

    /** 游戏线程纯字段校验；不加载Pawn类或依赖资产。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};
