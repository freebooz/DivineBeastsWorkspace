#pragma once

#include "Loading/GamePlatformLoadingScreenService.h"
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "DivineBeastsLoadingViewModel.generated.h"

/**
 * UDivineBeastsLoadingViewModel（神兽联盟加载视图模型）。
 *
 * 职责：
 * - 在页面激活期间订阅 GamePlatformLoadingScreenService（平台加载界面服务）。
 * - 保存最近一次真实 Loading Snapshot（加载快照），通过 ViewModel 事件通知页面更新。
 * - 未知进度保持 Progress < 0，禁止伪造百分比。
 *
 * 性能策略：
 * - 只在 ViewModel 页面生命周期内绑定一个动态委托。
 * - 相同快照不重复 MarkStateChanged，减少 Blueprint 重绘和无效布局。
 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsLoadingViewModel
    : public UDivineBeastsUIViewModel
{
    GENERATED_BODY()

public:
    /**
     * 注入当前 LocalPlayer 使用的平台 Loading Service。
     * 服务由 UI Manager 创建，本 ViewModel 不拥有其生命周期。
     */
    void InitializeLoadingService(
        UGamePlatformLoadingScreenService* InLoadingService);

    /** 返回最近一次真实加载快照。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Loading")
    FGamePlatformUILoadingSnapshot GetLoadingSnapshot() const
    {
        return LoadingSnapshot;
    }

protected:
    /** 页面激活时订阅 Loading Service 并抓取一次初始快照。 */
    virtual void OnPageBegan() override;

    /** 页面失活时立即解绑 Loading Service。 */
    virtual void OnPageEnded() override;

private:
    /** Loading Service 快照变化事件回调。 */
    UFUNCTION()
    void HandleLoadingSnapshotChanged(
        const FGamePlatformUILoadingSnapshot& Snapshot);

    /** 绑定平台加载服务事件；重复调用安全。 */
    void BindLoadingEvents();

    /** 解绑平台加载服务事件；重复调用安全。 */
    void UnbindLoadingEvents();

    /** 判断两个加载快照是否等价，用于抑制无意义 UI 刷新。 */
    static bool AreSnapshotsEquivalent(
        const FGamePlatformUILoadingSnapshot& A,
        const FGamePlatformUILoadingSnapshot& B);

    /** 当前 LocalPlayer 的平台加载服务。 */
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformLoadingScreenService> LoadingService = nullptr;

    /** 最近一次对 UI 可见的加载快照。 */
    UPROPERTY(Transient)
    FGamePlatformUILoadingSnapshot LoadingSnapshot;

    /** 防止同一页面生命周期重复绑定 Loading 委托。 */
    bool bLoadingEventsBound = false;
};
