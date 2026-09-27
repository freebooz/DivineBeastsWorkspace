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
    bool TickProjection(float DeltaSeconds);
    UWorld* GetServiceWorld() const;

    UPROPERTY(Transient)
    TObjectPtr<ULocalPlayer> LocalPlayer = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUILayerStack> RootLayout = nullptr;

    TMap<FGuid, TWeakObjectPtr<UGamePlatformWorldWidgetBase>> ActiveWidgets;
    TArray<TWeakObjectPtr<UGamePlatformWorldWidgetBase>> PooledWidgets;
    FTSTicker::FDelegateHandle ProjectionTickerHandle;
};
