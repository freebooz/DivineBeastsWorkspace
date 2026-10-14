#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "Engine/Texture2D.h"
#include "GamePlatformSlotWidget.generated.h"

/** FGamePlatformUISlotState（游戏平台通用槽位显示状态）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUISlotState
{
    GENERATED_BODY()

    /** 槽位稳定身份，例如Quickbar.0、Ability.Primary；平台不解释业务语义。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    FName SlotId = NAME_None;

    /** 当前内容显示身份；空表示槽位为空。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    FName ContentId = NAME_None;

    /** 图标软引用；禁止槽位控件同步加载资源。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    TSoftObjectPtr<UTexture2D> Icon;

    /** 可选数量/充能次数；小于0表示不显示。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    int32 Count = INDEX_NONE;

    /** 0～1归一化覆盖比例，可用于冷却/进度遮罩。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    float OverlayProgress = 0.0f;

    /** 当前槽位是否允许交互。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    bool bEnabled = true;

    /** 是否处于Pending（待确认）状态。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    bool bPending = false;
};

/** 中立槽位调用事件；只传当前显示身份，具体授权、输入和业务结果由消费者拥有。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FGamePlatformUISlotInvokedNative, FName);

/**
 * UGamePlatformSlotWidget（游戏平台通用槽位控件）。
 *
 * 背包物品槽、装备槽、技能槽、快捷栏均可复用本视觉原子。
 * 业务差异由领域ViewModel映射成FGamePlatformUISlotState，不在平台UI中建立业务继承树。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformSlotWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()

public:
    /** 游戏线程拷贝领域显示快照并将覆盖比例裁剪为0～1；仅变化时通知蓝图，不加载图标或修改领域状态。 */
    UFUNCTION(BlueprintCallable, Category="UI|Slot")
    void ApplySlotState(const FGamePlatformUISlotState& InState);

    /** 事件驱动的纯视觉覆盖时序：截止时间为Slate实时时钟秒，Duration为总秒数。
     * 无有效时序时清除遮罩。只设置本实例材质参数；不Tick、不查询业务、不提前恢复交互资格。 */
    void ApplyOverlayTiming(float EndsAtAppSeconds, float DurationSeconds);

    /** 返回本Widget当前只读显示快照的值拷贝；空内容不代表拥有任何业务授权。 */
    UFUNCTION(BlueprintPure, Category="UI|Slot")
    FGamePlatformUISlotState GetSlotState() const { return SlotState; }

    /** 游戏线程提交当前槽位的交互意图。空槽、禁用、待确认或无消费者返回false，不修改业务状态。
     * 返回true只表示同步调用了消费者；不代表背包操作或服务器施法已经完成。 */
    bool RequestSlotInvocation();

    /** 消费者按Widget生命周期绑定/解绑；此Widget销毁显示时清理本实例订阅，不持有全局业务对象。 */
    FGamePlatformUISlotInvokedNative& OnSlotInvoked() { return SlotInvoked; }

protected:
    /** 左键形成中立交互意图，其他按键及拒绝状态保留父类处理；不捕获鼠标或构造游戏输入映射。 */
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    /** 显示生命周期结束时清理本Widget同步订阅；再次显示由页面重新绑定，不结束领域业务。 */
    virtual void NativeDestruct() override;

    /** 游戏线程发生显示快照变化后的视觉通知；蓝图只投影样式，禁止在此授予权威数据。 */
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Slot", meta=(DisplayName="槽位状态已变化"))
    void BP_OnSlotStateChanged(FGamePlatformUISlotState State);

private:
    /** 当前视图的瞬态显示快照，由消费者拥有领域真源；不跨页持久化或通过网络复制。 */
    UPROPERTY(Transient)
    FGamePlatformUISlotState SlotState;

    FGamePlatformUISlotInvokedNative SlotInvoked;
};
