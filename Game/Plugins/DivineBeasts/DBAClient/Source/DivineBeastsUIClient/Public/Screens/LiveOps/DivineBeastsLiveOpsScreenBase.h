#pragma once

#include "Contracts/Domains/DivineBeastsLiveOpsUIContracts.h"
#include "Screens/DivineBeastsUIScreen.h"
#include "Components/ComboBoxString.h"
#include "DivineBeastsLiveOpsScreenBase.generated.h"

class UDivineBeastsLiveOpsViewModel;

/** 项目公告/活动/邮件的共同页面基类；商城支付流程继续归CommerceUI所有。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsLiveOpsScreenBase : public UDivineBeastsUIScreen
{
    GENERATED_BODY()
public:
    UDivineBeastsLiveOpsScreenBase() { UIDomain = EDivineBeastsUIDomain::LiveOps; }
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|LiveOps")
    UDivineBeastsLiveOpsViewModel* GetLiveOpsViewModel() const;
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|LiveOps")
    FDivineBeastsLiveOpsUIProjection GetLiveOpsProjection() const;
protected:
    /** 平台统一筛选激活代次；本页仅绑定浏览命令，不联网、不设置已读或发奖。 */
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    virtual void RefreshInitialState() override;
    virtual void OnViewModelStateChanged(int32 Revision,int32 PageGeneration) override;
private:
    TArray<FName> NoticeIds;
    FName SelectedNoticeId;
    bool bRefreshingNotices=false;
    void RefreshNoticePresentation();
    UFUNCTION() void ShowNotices();
    UFUNCTION() void NoticeSelected(FString Option,ESelectInfo::Type SelectionType);
    UFUNCTION() void CloseNotices();
};
