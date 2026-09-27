#pragma once

#include "Screens/GamePlatformUIScreen.h"
#include "GamePlatformMobaArenaScreenBase.generated.h"

class UGamePlatformArenaViewModel;

/**
 * UGamePlatformMobaArenaScreenBase（MOBA通用竞技页面基类）。
 *
 * 供匹配、准备、英雄选择、记分板和赛后结果等项目页面复用平台页面生命周期与竞技只读ViewModel。
 * 页面只负责表现和用户意图入口，不决定匹配算法、比分或胜负。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMARENACLIENT_API UGamePlatformMobaArenaScreenBase
    : public UGamePlatformUIScreen
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Arena|UI")
    UGamePlatformArenaViewModel* GetArenaViewModel() const;
};
