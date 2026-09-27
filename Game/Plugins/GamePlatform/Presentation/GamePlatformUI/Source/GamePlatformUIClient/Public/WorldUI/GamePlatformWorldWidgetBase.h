#pragma once

#include "Core/GamePlatformWidgetBase.h"
#include "Requests/GamePlatformWorldUIRequest.h"
#include "GamePlatformWorldWidgetBase.generated.h"

/**
 * UGamePlatformWorldWidgetBase（游戏平台世界投影界面基类）。
 *
 * 所有名称板、世界血条、任务/队伍/导航标记都通过 WorldUIService 集中投影。
 * 本类禁止自身 Tick，避免大量实体各自执行世界坐标到屏幕坐标转换。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformWorldWidgetBase
    : public UGamePlatformWidgetBase
{
    GENERATED_BODY()

public:
    /** 应用注册/更新后的只读世界UI请求。 */
    void ApplyWorldUIRequest(const FGamePlatformWorldUIRequest& InRequest);

    /** 服务统一投影后写入屏幕位置和可见性。 */
    void SetProjectedState(
        FVector2D InScreenPosition,
        bool bInVisible,
        float InDistanceCentimeters);

    /** 回收到对象池前清理状态。 */
    void ResetWorldUIState();

    const FGamePlatformWorldUIRequest& GetWorldUIRequest() const
    {
        return WorldUIRequest;
    }

protected:
    UFUNCTION(BlueprintImplementableEvent, Category="UI|WorldUI", meta=(DisplayName="世界UI请求已应用"))
    void BP_OnWorldUIRequestApplied(FGamePlatformWorldUIRequest Request);

    UFUNCTION(BlueprintImplementableEvent, Category="UI|WorldUI", meta=(DisplayName="世界UI投影已更新"))
    void BP_OnWorldUIProjected(
        FVector2D ScreenPosition,
        bool bVisible,
        float DistanceCentimeters);

    UFUNCTION(BlueprintImplementableEvent, Category="UI|WorldUI", meta=(DisplayName="世界UI控件即将回收"))
    void BP_OnWorldUIRecycled();

private:
    UPROPERTY(Transient)
    FGamePlatformWorldUIRequest WorldUIRequest;
};
