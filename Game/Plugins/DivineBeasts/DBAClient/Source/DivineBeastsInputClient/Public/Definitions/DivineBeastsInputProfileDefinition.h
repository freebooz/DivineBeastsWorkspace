#pragma once

#include "Definitions/GamePlatformInputProfileDefinition.h"
#include "DivineBeastsInputProfileDefinition.generated.h"

/**
 * UDivineBeastsInputProfileDefinition（神兽联盟输入配置定义）。
 * 只增加项目级完整性约束，不重写平台输入运行时；纯按键/设备差异仍使用DataAsset数据实例。
 */
UCLASS(BlueprintType)
class DIVINEBEASTSINPUTCLIENT_API UDivineBeastsInputProfileDefinition
    : public UGamePlatformInputProfileDefinition
{
    GENERATED_BODY()

public:
    /** Gameplay Profile是否必须包含神兽联盟核心攻击/四技能槽/锁定语义。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|Input")
    bool bRequireCoreGameplaySemantics = true;

    virtual FGamePlatformResult ValidateDefinition() const override;
};
