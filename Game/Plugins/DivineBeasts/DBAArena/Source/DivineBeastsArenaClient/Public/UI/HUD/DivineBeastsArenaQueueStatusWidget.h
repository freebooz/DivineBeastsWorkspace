#pragma once

#include "UI/GamePlatformMobaArenaHUDBase.h"
#include "DivineBeastsArenaQueueStatusWidget.generated.h"

/** 村庄非模态竞技状态：关闭完整面板后保留真实状态、重开和取消入口；不改变输入模式或业务权威。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSARENACLIENT_API UDivineBeastsArenaQueueStatusWidget : public UGamePlatformMobaArenaHUDBase
{
    GENERATED_BODY()

protected:
    /** 首次快照加本地玩家状态事件；构造/销毁成对订阅，无Tick或业务计时器。 */
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    virtual void RefreshInitialState() override;
    virtual void OnViewModelStateChanged(int32 Revision,int32 PageGeneration) override;

private:
    UFUNCTION() void OpenMatchmaking();
    UFUNCTION() void CancelMatchmaking();
};
