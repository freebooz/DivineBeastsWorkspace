#pragma once

#include "Contracts/DivineBeastsUIContracts.h"
#include "HUD/DivineBeastsHUDWidget.h"
#include "DivineBeastsWorldHUDBase.generated.h"
class UGamePlatformUIManagerSubsystem;
class UGamePlatformUIScreen;

/** 开放世界、新手村、教学、训练HUD统一只读视图，不增加新的世界状态数据源。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsWorldHUDBase : public UDivineBeastsHUDWidget
{
    GENERATED_BODY()
public:
    /** 从平台已绑定的项目ViewModel获取当前世界状态。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|WorldHUD")
    FDivineBeastsUIWorldProjection GetWorldProjection() const;
    /** 已具备可展示的WorldId时返回true。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|WorldHUD")
    bool HasWorldView() const;
    /** 失去世界/模型时撤销本HUD尚未完成的页面打开；不关闭他人页、不撤业务事务。游戏线程，重复安全。 */
    void ClearWorldCommandContext();
protected:
    /** 命名按钮只在构造/销毁范围绑定；状态与输入设备由平台事件更新。 */
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    virtual void RefreshInitialState() override;
    virtual void OnViewModelStateChanged(int32 Revision,int32 PageGeneration) override;
    virtual void OnAdaptiveContextChanged(const FGamePlatformUIAdaptiveContext& Context) override;
private:
    /** 不扫描Actor/GAS；仅将当前只读世界投影填入Monolith控件。 */
    void RefreshWorldPresentation();
    UFUNCTION() void OpenJournal();
    UFUNCTION() void OpenParty();
    UFUNCTION() void ResetTraining();
    UFUNCTION() void OpenQuestLog();
    UFUNCTION() void OpenSocial();
    UFUNCTION() void OpenNotices();
    /** 保存本HUD提交的异步打开身份；退出时只取消这些在途请求，不操作其他来源页面。 */
    TMap<FName,FGuid> OwnedUtilityRequests;
    TWeakObjectPtr<UGamePlatformUIManagerSubsystem> UtilityUIManager;
    FName DispatchingUtilityScreen;
    FGuid SynchronousUtilityCompletion;
    bool bWorldControlsBound=false;
    uint64 WorldControlGeneration=0;
    /** 只允许仍构造、可见且持有活动模型的HUD提交用户命令。 */
    bool CanSubmitWorldCommand() const;
    UFUNCTION() void UtilityOpened(FGuid RequestId,FName ScreenId,UGamePlatformUIScreen* Screen);
    UFUNCTION() void UtilityFailed(FGuid RequestId,FName ScreenId,FText Reason);
    void OpenUtility(FName ScreenId);
    /** 最近本页命令受理反馈，不代表服务器完成或训练成功。 */
    FText WorldCommandFeedback;
};
