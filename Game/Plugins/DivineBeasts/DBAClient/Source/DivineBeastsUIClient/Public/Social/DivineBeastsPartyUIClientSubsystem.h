// 项目公共小队UI组合根：LocalPlayer作用域，拥有五页软路由、在途打开请求与原生命令绑定；不依赖竞技。
#pragma once
#include "Subsystems/LocalPlayerSubsystem.h"
#include "DivineBeastsPartyUIClientSubsystem.generated.h"

class UGamePlatformUIScreen;
class UGamePlatformUIScreenDefinition;
class UGamePlatformUIManagerSubsystem;
class UDivineBeastsPartyViewModel;
class UDivineBeastsPartyScreenBinding;

/** 可选活动入口；竞技插件仅以公开中立回调注册，不使公共UI反向依赖MOBA。 */
// PartyId为空表示授权快照确认的单人；有队伍时必须传原子名单身份，不能悄悄降级单人报名。
using FDivineBeastsPartyActivityAction=TFunction<bool(const FString& PartyId)>;

/** 普通小队页面装配；只开放真实Monolith资产，加载失败由平台事件返回，不在运行时造占位页面。 */
UCLASS()
class DIVINEBEASTSUICLIENT_API UDivineBeastsPartyUIClientSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    /** 换控制器/地图时清理本组合根请求、旧目标和页面，不关闭其他来源的界面。 */
    virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
    /** 打开已注册公共页面；默认面板，不修改队伍。 */
    UFUNCTION(BlueprintCallable,Category="DivineBeasts|Party") bool OpenPartyPage(FName PageId=NAME_None);
    /** 世界交互适配传入可信公开目标；Revision未知(-1)时申请不可用，绝不盲写版本0。 */
    bool OpenPlayerContext(const FString& TargetPlayerId,const FString& TargetPartyId,int64 TargetRevision);
    /** 同一LocalPlayer只允许一个活动提供者，弱所有者失效后自动不可用；注销严格匹配句柄。 */
    FGuid RegisterActivity(UObject* Owner,FDivineBeastsPartyActivityAction Action);
    bool UnregisterActivity(FGuid Handle);
    bool OpenActivity(const FString& PartyId);
    bool HasActivity() const;
    /** 页面资源错误为安全文本，不包含路径或传输秘密。 */
    UPROPERTY(BlueprintReadOnly,Transient) FText LastError;
private:
    UFUNCTION() void ScreenOpened(FGuid RequestId,FName ScreenId,UGamePlatformUIScreen* Screen);
    UFUNCTION() void ScreenFailed(FGuid RequestId,FName ScreenId,FText Reason);
    /** 兼容ID通知只触发精确实例/原栈复核，不能按同名ID清除仍在栈内的其他页面。 */
    UFUNCTION() void ScreenClosed(FName ScreenId);
    UPROPERTY(Transient) TObjectPtr<UGamePlatformUIManagerSubsystem> UI;
    UPROPERTY(Transient) TArray<TObjectPtr<UGamePlatformUIScreenDefinition>> Definitions;
    UPROPERTY(Transient) TMap<FGuid,TObjectPtr<UDivineBeastsPartyViewModel>> PendingModels;
    UPROPERTY(Transient) TArray<TObjectPtr<UDivineBeastsPartyScreenBinding>> Bindings;
    /** 平台打开允许同步回调；先强持有本次模型，再按返回请求身份核对同步完成。 */
    UPROPERTY(Transient) TObjectPtr<UDivineBeastsPartyViewModel> DispatchingModel;
    TSet<FGuid> OpenedDuringDispatch;
    TSet<FGuid> FailedDuringDispatch;
    void CloseOwnedPages();
    TWeakObjectPtr<UObject> ActivityOwner;
    FDivineBeastsPartyActivityAction ActivityAction;
    FGuid ActivityHandle;
    FString ContextPlayer;
    FString ContextParty;
    int64 ContextRevision=-1;
    bool bClosing=false;
};
