#pragma once

#include "Screens/GamePlatformModalScreen.h"
#include "GamePlatformMobaArenaModalScreenBase.generated.h"

class UGamePlatformArenaViewModel;

/**
 * UGamePlatformMobaArenaModalScreenBase（MOBA通用竞技模态页面基类）。
 *
 * 用于 Match Found / Ready Check（匹配成功/准备确认）等必须阻断下层输入的竞技页面。
 * 复用平台 Modal 生命周期，并提供类型安全的竞技 ViewModel 访问。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMARENACLIENT_API UGamePlatformMobaArenaModalScreenBase
    : public UGamePlatformModalScreen
{
    GENERATED_BODY()

public:
    /** 返回平台竞技只读 ViewModel；类型不匹配时返回 nullptr。 */
    UFUNCTION(BlueprintPure, Category="Arena|UI")
    UGamePlatformArenaViewModel* GetArenaViewModel() const;
};
