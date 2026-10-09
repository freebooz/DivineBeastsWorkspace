#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "Components/GamePlatformPortraitWidget.h"
#include "Components/GamePlatformResourceBarWidget.h"
#include "GamePlatformTargetFrameWidget.generated.h"

/**
 * FGamePlatformUITargetFrameState（目标、焦点或首领状态框只读投影）。
 *
 * 服务端及领域客户端必须先按观察者可见性过滤生命/盾/身份；
 * UI仅组合已有Portrait（肖像）和ResourceBar（资源条），不扫描敌方ASC/Actor。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUITargetFrameState
{
    GENERATED_BODY()

    /** 所属本地观察上下文，换地图/角色后必须更新。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Target")
    FGuid SourceScopeId;

    /** 当前目标公开的稳定显示身份，不能使用显示名称替代作用域身份。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Target")
    FName TargetDisplayId = NAME_None;

    /** 同一个观察上下文的递增版本，用于淘汰迟到的目标事件。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Target")
    int64 Revision = -1;

    /** false表示当前目标对观察者已不可见，界面必须清除旧目标详情。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Target")
    bool bVisible = false;

    /** 目标肖像与本地化名称，只接受经过授权的显示数据。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Target")
    FGamePlatformUIPortraitState Portrait;

    /** 目标已许可展示的生命值，允许业务用归一化/模糊值而非精确真值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Target")
    FGamePlatformUIResourceBarState Health;

    /** 目标已许可展示的护盾。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Target")
    FGamePlatformUIResourceBarState Shield;

    /** 阵营视觉标记，不触发敌对判定。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Target")
    bool bHostile = false;

    /** 首领特殊样式标记，不代表平台知道具体Boss玩法。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Target")
    bool bBoss = false;

    /** 权威死亡状态已传给当前观察者。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Target")
    bool bDead = false;
};

/** FGamePlatformUITargetFramePresentation（目标快照中立合法性检查）。 */
struct GAMEPLATFORMUICLIENT_API FGamePlatformUITargetFramePresentation
{
    /** 验证来源、目标与资源数值合法；隐藏目标允许没有资源细节。 */
    static bool IsValidSnapshot(const FGamePlatformUITargetFrameState& InState);
};

/**
 * UGamePlatformTargetFrameWidget（通用目标状态/焦点框/首领状态框）。
 * 统一数据源作用域和单调修订；子控件可选绑定，项目层定义差异美术。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformTargetFrameWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()

public:
    /** 绑定当前观察者数据源；换源时清空此前目标。 */
    UFUNCTION(BlueprintCallable, Category="UI|Target")
    bool BindTargetSource(FGuid InSourceScopeId);

    /** 按已授权状态刷新目标；隐藏状态将清除旧生命/肖像缓存。 */
    UFUNCTION(BlueprintCallable, Category="UI|Target")
    bool ApplyTargetSnapshot(const FGamePlatformUITargetFrameState& InState);

    /** 退出世界/失活时清空并解绑，不影响Gameplay的锁定目标行为。 */
    UFUNCTION(BlueprintCallable, Category="UI|Target")
    void ClearTargetSource();

    /** 当前允许展示的目标状态；隐藏目标返回仅含作用域/版本的空视图。 */
    UFUNCTION(BlueprintPure, Category="UI|Target")
    FGamePlatformUITargetFrameState GetTargetSnapshot() const { return State; }

protected:
    /** 由内容蓝图选择是否组合标准头像视觉原子。 */
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="UI|Target")
    TObjectPtr<UGamePlatformPortraitWidget> TargetPortrait = nullptr;

    /** 生命条为可选控件，使用平台现有ResourceBar原子。 */
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="UI|Target")
    TObjectPtr<UGamePlatformResourceBarWidget> TargetHealthBar = nullptr;

    /** 护盾条为可选控件。 */
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="UI|Target")
    TObjectPtr<UGamePlatformResourceBarWidget> TargetShieldBar = nullptr;

    UFUNCTION(BlueprintImplementableEvent, Category="UI|Target",
        meta=(DisplayName="目标状态显示已变化"))
    void BP_OnTargetFrameChanged(FGamePlatformUITargetFrameState UpdatedState);

private:
    /** 撤销旧头像、生命及护盾控件的缓存，迷雾/失活后不可继续泄露旧目标数据。 */
    void ResetTargetVisuals();

    /** 当前数据源身份。 */
    UPROPERTY(Transient)
    FGuid BoundScopeId;

    /** 最新授权快照；隐藏时必须销毁之前的私密目标字段。 */
    UPROPERTY(Transient)
    FGamePlatformUITargetFrameState State;
};
