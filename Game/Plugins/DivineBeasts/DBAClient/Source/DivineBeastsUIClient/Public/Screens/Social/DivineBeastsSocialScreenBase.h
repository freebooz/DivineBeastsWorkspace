#pragma once

#include "Contracts/Domains/DivineBeastsSocialUIContracts.h"
#include "Screens/DivineBeastsUIScreen.h"
#include "Components/ComboBoxString.h"
#include "DivineBeastsSocialScreenBase.generated.h"

class UDivineBeastsSocialViewModel;

/** 社交与组队页面基类：显示授权快照，交互命令由独立社交适配器处理。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsSocialScreenBase : public UDivineBeastsUIScreen
{
    GENERATED_BODY()
public:
    UDivineBeastsSocialScreenBase() { UIDomain = EDivineBeastsUIDomain::Social; }
    /** 类型安全地取得当前社交ViewModel。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Social")
    UDivineBeastsSocialViewModel* GetSocialViewModel() const;
    /** 服务未接入时返回不可用的空数据，而非伪造好友或队友。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Social")
    FDivineBeastsSocialUIProjection GetSocialProjection() const;
protected:
    /** 激活期间绑定本页浏览命令；平台持有唯一VM状态委托。 */
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    virtual void RefreshInitialState() override;
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) override;
private:
    /** 本地选择只是可见名单索引，写业务命令必须由独立小队页面重新核对公开身份和版本。 */
    FString SelectedPlayerId;
    TArray<FString> PlayerIds;
    bool bRefreshingFriends = false;
    void RefreshSocialPresentation();
    UFUNCTION() void ShowFriends();
    UFUNCTION() void OpenParty();
    UFUNCTION() void FriendSelected(FString Option, ESelectInfo::Type SelectionType);
    UFUNCTION() void CloseSocialPage();
};
