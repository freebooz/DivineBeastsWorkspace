#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/PrimaryAssetId.h"
#include "DivineBeastsArenaCombatFeedbackSettings.generated.h"

/**
 * 神兽联盟竞技客户端的打击反馈装配配置。
 * 只有实际编辑器创建并登记为GamePlatformDefinition的目录资产才能填写该ID。
 * 空值表示功能不装配，绝不凭文件名猜测或同步加载目录。
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="DivineBeasts Arena Combat Feedback"))
class UDivineBeastsArenaCombatFeedbackSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** 命中反馈目录的GamePlatformDefinition主资产身份；无正式资产时保持为空。 */
    UPROPERTY(Config, EditAnywhere, Category="DivineBeasts|Combat Feedback")
    FPrimaryAssetId CatalogDefinitionId;
};
