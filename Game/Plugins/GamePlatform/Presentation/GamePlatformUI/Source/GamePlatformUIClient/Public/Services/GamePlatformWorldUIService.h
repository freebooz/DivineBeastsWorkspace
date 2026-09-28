#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Requests/GamePlatformWorldUIRequest.h"
#include "UObject/Object.h"
#include "GamePlatformWorldUIService.generated.h"

class UGamePlatformUILayerStack;
class UGamePlatformWorldWidgetBase;
class ULocalPlayer;

/**
 * UGamePlatformWorldUIService（游戏平台世界投影UI服务）。
 *
 * 统一维护名称板、世界血条和世界标记的世界坐标→屏幕坐标投影。
 * 使用单一低频 Ticker（默认30Hz）集中更新，避免每个世界UI Widget各自Tick。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformWorldUIService : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(ULocalPlayer* InLocalPlayer);
    void Deinitialize();
    void SetRootLayout(UGamePlatformUILayerStack* InRootLayout);

    UFUNCTION(BlueprintCallable, Category="UI|WorldUI")
    FGuid RegisterWorldUI(
        FGamePlatformWorldUIRequest Request,
        TSubclassOf<UGamePlatformWorldWidgetBase> WidgetClass);

    UFUNCTION(BlueprintCallable, Category="UI|WorldUI")
    bool UpdateWorldUI(
        FGuid RequestId,
        FGamePlatformWorldUIRequest Request);

    UFUNCTION(BlueprintCallable, Category="UI|WorldUI")
    bool UnregisterWorldUI(FGuid RequestId);

    void Clear();

private:
    UGamePlatformWorldWidgetBase* AcquireWidget(
        TSubclassOf<UGamePlatformWorldWidgetBase> WidgetClass);
    void RecycleWidget(UGamePlatformWorldWidgetBase* Widget);
    /** 仅在存在活动世界UI时启动30Hz集中投影Ticker，空闲时完全停止。 */
    void EnsureProjectionTicker();

    /** 主动停止集中投影Ticker；Deinitialize/清空最后一个实例时调用。 */
    void StopProjectionTicker();
    bool TickProjection(float DeltaSeconds);
    UWorld* GetServiceWorld() const;

    UPROPERTY(Transient)
    TObjectPtr<ULocalPlayer> LocalPlayer = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUILayerStack> RootLayout = nullptr;

    TMap<FGuid, TWeakObjectPtr<UGamePlatformWorldWidgetBase>> ActiveWidgets;

    /** 池内Widget由服务强引用保活；活动Widget仍由WorldProjectionLayer持有。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UGamePlatformWorldWidgetBase>> PooledWidgets;
    FTSTicker::FDelegateHandle ProjectionTickerHandle;
};
