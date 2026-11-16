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
    /** 旧Profile兼容：要求基础攻击、Slot1..Slot4与目标锁定。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|Input")
    bool bRequireCoreGameplaySemantics = true;
    /** 新八槽Profile显式开启：额外要求Slot5..Slot7，旧已保存资产不被强行升级。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|Input")
    bool bRequireEightActiveSlots = false;

    virtual FGamePlatformResult ValidateDefinition() const override;
};
