// 本文件属于平台客户端UI回归夹具；只创建Transient C++对象，不生成/修改Widget Blueprint或生产视觉资产。
// 夹具驱动真实生命周期/动态结果委托，不提供固定成功替身；中文说明见本插件Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Core/GamePlatformActivatableWidgetBase.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "ViewModels/GamePlatformViewModelBase.h"
#include "GamePlatformUIReentryFixture.generated.h"

/** 只用于引擎CreateWidget路径的具体空树夹具；不注册为项目Root、不产生可交付布局资产。 */
UCLASS(Transient, NotBlueprintable)
class UGamePlatformUIReentryRootFixture final : public UGamePlatformUILayerStack
{
    GENERATED_BODY()
};

/** 同步结果观察者；回调真实Deinitialize以暴露广播后的旧指针/重新发布错误。 */
UCLASS(Transient, NotBlueprintable)
class UGamePlatformUIReentryObserver final : public UObject
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<UGamePlatformUIManagerSubsystem> Manager;
    int32 Notifications = 0;
    UFUNCTION() void HandleClosed(FName ScreenId)
    { (void)ScreenId; ++Notifications; if (Manager.IsValid()) Manager->Deinitialize(); }
    UFUNCTION() void HandleFailed(FGuid RequestId, FName ScreenId, FText Reason)
    { (void)RequestId; (void)ScreenId; (void)Reason; ++Notifications; if (Manager.IsValid()) Manager->Deinitialize(); }
};

/** 页面钩子夹具；调用者显式选择自闭/嵌套替换输入，所有状态仍由生产VM实现维护。 */
UCLASS(Transient, NotBlueprintable)
class UGamePlatformUIReentryViewModel final : public UGamePlatformViewModelBase
{
    GENERATED_BODY()
public:
    TFunction<void()> BeganHook;
    TFunction<void()> EndedHook;
protected:
    virtual void OnPageBegan() override { if (BeganHook) BeganHook(); }
    virtual void OnPageEnded() override { if (EndedHook) EndedHook(); }
};

/** 真实CommonUI激活基类的具体测试页；统计刷新，提供同步自闭钩子，无业务请求/轮询。 */
UCLASS(Transient, NotBlueprintable)
class UGamePlatformUIReentryWidget final : public UGamePlatformActivatableWidgetBase
{
    GENERATED_BODY()
public:
    int32 StateEvents = 0;
    int32 InitialRefreshes = 0;
    TFunction<void()> RefreshHook;
protected:
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) override
    { (void)Revision; (void)PageGeneration; ++StateEvents; }
    virtual void RefreshInitialState() override
    { ++InitialRefreshes; if (RefreshHook) RefreshHook(); }
};
