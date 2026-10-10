// 本文件属于GamePlatform平台层 GamePlatformUI，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GamePlatformViewModelBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FGamePlatformUIViewStateChanged,
    int32, Revision,
    int32, PageGeneration);

/**
 * UGamePlatformViewModelBase（游戏平台事件驱动 ViewModel 基类）。
 *
 * 提供 Revision（修订号）、PageGeneration（页面代次）、激活状态和统一状态变化事件。
 * 不强制依赖 UE MVVM Beta；业务 ViewModel 可以通过 OnPageBegan / OnPageEnded
 * 在页面生命周期内订阅真实服务事件，并在失活时立即解绑。
 *
 * 性能约束：
 * - 不提供 Tick。
 * - 仅状态真实变化时由派生类调用 MarkStateChanged。
 * - 页面代次用于拒绝迟到异步回调，避免无效界面刷新。
 */
UCLASS(BlueprintType, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformViewModelBase : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="UI|ViewModel")
    void BeginPage();

    UFUNCTION(BlueprintCallable, Category="UI|ViewModel")
    void EndPage();

    UFUNCTION(BlueprintCallable, Category="UI|ViewModel")
    void MarkStateChanged();

    UFUNCTION(BlueprintPure, Category="UI|ViewModel")
    bool IsCallbackCurrent(int32 ExpectedRevision, int32 ExpectedPageGeneration) const;

    UFUNCTION(BlueprintPure, Category="UI|ViewModel")
    int32 GetRevision() const { return Revision; }

    UFUNCTION(BlueprintPure, Category="UI|ViewModel")
    int32 GetPageGeneration() const { return PageGeneration; }

    UFUNCTION(BlueprintPure, Category="UI|ViewModel")
    bool IsPageActive() const { return bPageActive; }

    UPROPERTY(BlueprintAssignable, Category="UI|ViewModel")
    FGamePlatformUIViewStateChanged OnViewStateChanged;

protected:
    /**
     * 页面进入激活状态后的扩展点。
     * 派生 ViewModel 可在此订阅服务事件并抓取一次初始快照。
     */
    virtual void OnPageBegan() {}

    /**
     * 页面旧代次已失效后的清理扩展点；不依赖IsPageActive=true作为解绑前提。
     * 派生 ViewModel 必须在此解绑自身订阅，避免不可见页面持续接收事件。
     */
    virtual void OnPageEnded() {}

private:
    /** 当前视图状态修订号；每次 MarkStateChanged 单调递增。 */
    UPROPERTY(Transient)
    int32 Revision = 0;

    /** 页面代次；每次进入或退出页面都会变化，用于淘汰迟到异步回调。 */
    UPROPERTY(Transient)
    int32 PageGeneration = 0;

    /** 当前 ViewModel 是否处于可见页面生命周期。 */
    UPROPERTY(Transient)
    bool bPageActive = false;
};
