#pragma once

#include "Screens/GamePlatformHUDWidget.h"
#include "GamePlatformMobaArenaHUDBase.generated.h"

class UGamePlatformArenaViewModel;

/**
 * UGamePlatformMobaArenaHUDBase（MOBA通用竞技HUD基类）。
 *
 * 提供所有MOBA项目共用的竞技ViewModel绑定语义，不包含英雄、美术或具体项目规则。
 * 项目层 Arena HUD 只需要继承本类并提供视觉组合，不重新实现竞技状态读取。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMARENACLIENT_API UGamePlatformMobaArenaHUDBase
    : public UGamePlatformHUDWidget
{
    GENERATED_BODY()

public:
    /** 绑定MOBA竞技只读ViewModel；复用平台Widget事件生命周期。 */
    UFUNCTION(BlueprintCallable, Category="Arena|UI")
    void InitializeArenaViewModel(UGamePlatformArenaViewModel* InViewModel);

    /** 返回类型安全的竞技ViewModel。 */
    UFUNCTION(BlueprintPure, Category="Arena|UI")
    UGamePlatformArenaViewModel* GetArenaViewModel() const;
};
