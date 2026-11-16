#pragma once

#include "CoreMinimal.h"
#include "Requests/GamePlatformWorldUIRequest.h"
#include "UObject/Object.h"
#include "GamePlatformWorldUIService.generated.h"

class UGamePlatformUILayerStack;
class UGamePlatformWorldWidgetBase;
class ULocalPlayer;
class FViewport;
class APlayerController;

/**
 * UGamePlatformWorldUIService（游戏平台世界投影UI服务）。
 *
 * 统一维护名称板、世界血条和世界标记的世界坐标→屏幕坐标投影。
 * 位置、最终镜头与视口事件驱动集中投影；无Ticker或Widget业务轮询。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformWorldUIService : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(ULocalPlayer* InLocalPlayer);
    void Deinitialize();
    void SetRootLayout(UGamePlatformUILayerStack* InRootLayout);
    /** 根布局完成挂接后通知缓存提供者重试首次注册；调用方必须在失活时撤销订阅。 */
    FSimpleMulticastDelegate& OnRootLayoutChanged() { return RootLayoutChanged; }

    UFUNCTION(BlueprintCallable, Category="UI|WorldUI")
    FGuid RegisterWorldUI(
        FGamePlatformWorldUIRequest Request,
        TSubclassOf<UGamePlatformWorldWidgetBase> WidgetClass);

    UFUNCTION(BlueprintCallable, Category="UI|WorldUI")
    bool UpdateWorldUI(
        FGuid RequestId,
        FGamePlatformWorldUIRequest Request);

    /** 位移事件只写入锚点并重投影已有实例，保留已应用的内容。 */
    bool UpdateWorldUILocation(FGuid RequestId, const FVector& WorldLocation);

    UFUNCTION(BlueprintCallable, Category="UI|WorldUI")
    bool UnregisterWorldUI(FGuid RequestId);

    void Clear();

    /** 调用方最终相机事件重投影全部实例；位置事件可指定单个身份，不查询领域生命值。 */
    void RefreshProjection(FGuid OnlyRequestId = FGuid());

private:
    struct FActiveWorldWidget
    {
        TWeakObjectPtr<UGamePlatformWorldWidgetBase> Widget;
        uint64 Registration = 0;
    };

    UGamePlatformWorldWidgetBase* AcquireWidget(
        TSubclassOf<UGamePlatformWorldWidgetBase> WidgetClass);
    void RecycleWidget(UGamePlatformWorldWidgetBase* Widget, bool bAllowPooling = true);
    void ProjectWidget(FGuid RequestId, FActiveWorldWidget Active, const APlayerController& PlayerController,
        const FVector& CameraLocation, FIntPoint ViewportSize, uint64 LayoutGeneration);
    const FActiveWorldWidget* FindRegistration(FGuid RequestId, uint64 Registration) const;
    /** 仅活动期间订阅视口尺寸事件；相机与实体位置事件由调用方注入。 */
    void BindProjectionEvents();

    /** 清空最后一个实例及失活时解绑，防止旧世界回调。 */
    void UnbindProjectionEvents();
    void HandleViewportResized(FViewport* Viewport, uint32 Unused);
    UWorld* GetServiceWorld() const;

    UPROPERTY(Transient)
    TObjectPtr<ULocalPlayer> LocalPlayer = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUILayerStack> RootLayout = nullptr;

    TMap<FGuid, FActiveWorldWidget> ActiveWidgets;
    /** 同一请求ID及同一池对象可以立即复用，旧投影快照必须能识别这次替换。 */
    uint64 NextRegistration = 0;

    /** 池内Widget由服务强引用保活；活动Widget仍由WorldProjectionLayer持有。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UGamePlatformWorldWidgetBase>> PooledWidgets;
    FDelegateHandle ViewportResizedHandle;
    FSimpleMulticastDelegate RootLayoutChanged;
    /** 同步回调换Root或清理时阻断旧迁移栈，身份只在本服务内部使用。 */
    uint64 RootLayoutGeneration = 0;
    bool bClearing = false;
};
