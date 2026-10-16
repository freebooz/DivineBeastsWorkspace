// 每个公共小队页面唯一原生绑定；引用页面/VM均为弱引用，失活解除按钮/列表/状态委托，不拥有业务真源。
#pragma once
#include "UObject/Object.h"
#include "DivineBeastsPartyScreenBinding.generated.h"
class UGamePlatformUIScreen;
class UDivineBeastsPartyViewModel;
class UDivineBeastsPartyUIClientSubsystem;
class UCommonActivatableWidgetStack;
UCLASS()
class UDivineBeastsPartyScreenBinding : public UObject
{
    GENERATED_BODY()
    friend class FDivineBeastsPartyBindingRegressionTest;
public:
    void Bind(UGamePlatformUIScreen* Screen,UDivineBeastsPartyViewModel* Model,UDivineBeastsPartyUIClientSubsystem* Owner,
        UCommonActivatableWidgetStack* OriginalStack=nullptr);
    void Unbind();
    /** 仅关闭本绑定的精确实例，解绑后再关闭以允许外部关闭通知重入。 */
    void CloseOwnedPage();
    /** 对象有效不等于仍属于原页面代次；池化换模型后必须返回false。 */
    bool IsBound() const;
    UGamePlatformUIScreen* GetBoundScreen() const;
    UCommonActivatableWidgetStack* GetOriginalStack() const;
    UFUNCTION() void Refresh(int32 Revision=0,int32 Generation=0);
private:
    void Send(const FString& Action);
    void SelectItem(UObject* Item);
    void Activated();
    void Deactivated();
    /** 状态/选择事件设置引擎数值动画端点，不建立业务查询定时器。 */
    void UpdateDeadlineDisplay();
    /** 失活/销毁停止本页面引擎动画，不改变远端业务状态。 */
    void StopDeadlineAnimation();
    FString SelectionOwnerPlayerId;
    UFUNCTION() void Invite(); UFUNCTION() void Accept(); UFUNCTION() void Decline();
    UFUNCTION() void Leave(); UFUNCTION() void Transfer(); UFUNCTION() void Kick();
    UFUNCTION() void Disband(); UFUNCTION() void ToggleReady(); UFUNCTION() void Reload();
    UFUNCTION() void Close(); UFUNCTION() void OpenApplications(); UFUNCTION() void OpenInvitations();
    UFUNCTION() void OpenReady(); UFUNCTION() void RequestJoin(); UFUNCTION() void Activity();
    UFUNCTION() void Retry();
    UFUNCTION() void ConfirmReady();
    UFUNCTION() void DeclineReady();
    TWeakObjectPtr<UGamePlatformUIScreen> Screen;
    TWeakObjectPtr<UDivineBeastsPartyViewModel> Model;
    TWeakObjectPtr<UDivineBeastsPartyUIClientSubsystem> Owner;
    FString SelectedRequest;
    FString SelectedMember;
    TWeakObjectPtr<UCommonActivatableWidgetStack> OriginalStack;
    /** ClearListItems/恢复选中也产生选择事件；重建期间禁止递归刷新或覆盖用户稳定键。 */
    bool bRebuildingLists=false;
};
