// 项目层公共小队UI命令桥：本地玩家/页面作用域，向平台类型化Online提交命令；无竞技依赖、HTTP或业务Tick。
#pragma once
#include "ViewModels/Social/DivineBeastsSocialViewModel.h"
#include "Types/GamePlatformParty.h"
#include "Types/GamePlatformOnlineRequests.h"
#include "DivineBeastsPartyViewModel.generated.h"

class ULocalPlayer;
class UGamePlatformOnlineClientSubsystem;
class UGamePlatformPartyClientSubsystem;
struct FGamePlatformAuthSnapshot;

/** 公共小队页面视图模型；只保留授权投影、用户选择和在途身份，不拥有队伍权威。 */
UCLASS(BlueprintType)
class DIVINEBEASTSUICLIENT_API UDivineBeastsPartyViewModel : public UDivineBeastsSocialViewModel
{
    GENERATED_BODY()
public:
    /** 由LocalPlayer组合根连接，重复连接先撤销旧代次；不自动登录，不支持实例内多账号。 */
    void ConnectPlayer(ULocalPlayer* Player);
    /** 页面结束/组合根退出撤销本对象的所有请求和认证委托；不撤销远端已提交事务。 */
    void DisconnectPlayer();
    virtual void BeginDestroy() override;
    /** 显式完整重同步；正常更新来自平台共享观察通知，不建立页面定时查询。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Party") void RefreshParty();
    /** 提交当前页面明确动作；目标、队伍、请求和版本仍由平台/后端再次验证。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Party")
    bool SubmitPartyAction(const FString& Action, const FString& TargetPlayerId=FString(), const FString& RequestKey=FString());
    /** 显式回复服务器当前限时准备，不能用toggle把重复点击解释为反向意图。 */
    bool SubmitReadyResponse(bool bAccept);
    /** 对结果未知的原逻辑命令原键重试；不生成新键、不能绕过认证或忙碌门禁。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Party") bool RetryUnknownCommand();
    /** 当前版本可执行性仅供显示，服务端独立授权；空目标仍需在提交时校验。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Party") bool CanSubmitPartyAction(const FString& Action) const;
    /** 目标上下文来自世界交互适配的公开服务器身份，不从昵称或Actor地址推算。 */
    void SetTargetContext(const FString& PlayerId, const FString& PartyId, int64 Revision);
    /** 公开目标身份仅供上下文菜单说明，不含会话、地址或目标私有资料。 */
    FString GetTargetPlayerIdForDisplay() const { return TargetPlayer; }
    /** 仅供显示的服务器UTC估计；以已接收快照和本地单调时钟推进，不受用户调整系统时钟影响。 */
    FDateTime GetEstimatedServerTimeForDisplay() const;
    /** 只读快照；失败保留上次数据但StateKnown=false，不把旧名单当作当前真源。 */
    UPROPERTY(BlueprintReadOnly, Transient) FGamePlatformPartySnapshot Party;
    /** 本人有权限看到的真实邀请，最多20项。 */
    UPROPERTY(BlueprintReadOnly, Transient) TArray<FGamePlatformPartyRequest> Invitations;
    /** 本人公开身份，不包含会话或令牌。 */
    UPROPERTY(BlueprintReadOnly, Transient) FString CurrentPlayerId;
    /** 只有当前账号的成功快照才为true。 */
    UPROPERTY(BlueprintReadOnly, Transient) bool bStateKnown = false;
    /** 有读取或写入在途时禁用重复按钮。 */
    UPROPERTY(BlueprintReadOnly, Transient) bool bBusy = false;
    /** 是否保留需要显式读回/重试的未知写入。 */
    UPROPERTY(BlueprintReadOnly, Transient) bool bOutcomeUnknown = false;
    /** 本地化安全错误；不保存原始HTTP正文。 */
    UPROPERTY(BlueprintReadOnly, Transient) FText StatusText;
protected:
    virtual void OnPageBegan() override;
    virtual void OnPageEnded() override;
private:
    void AuthenticationChanged(const FGamePlatformAuthSnapshot& Snapshot);
    /** 仅消费同一认证代次的授权观察；页面失活、换账号后解绑，不取消公共订阅。 */
    void ObservationChanged(const FGamePlatformPartyResult& Result);
    void BindObservation();
    void UnbindObservation();
    void CancelOwnedRequests();
    bool Dispatch(const FGamePlatformPartyCommand& Command);
    bool ApplyParty(const FGamePlatformPartySnapshot& Next);
    void ReportFailure(const FGamePlatformPartyResult& Result);
    void PublishPartyProjection();
    /** 只借用已有GameInstance服务，不延长其生命周期。 */
    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> Online;
    TWeakObjectPtr<UGamePlatformPartyClientSubsystem> PartyClient;
    /** 页面请求绑定创建它的真实世界；旧世界退出由平台自动取消。 */
    TWeakObjectPtr<UWorld> RequestWorld;
    FDelegateHandle AuthHandle;
    FDelegateHandle ObservationHandle;
    TArray<FGamePlatformOnlineRequestHandle> OwnedRequests;
    FGuid AuthScope;
    uint64 OperationGeneration = 0;
    int64 ProjectionRevision = 0;
    FGamePlatformPartyCommand UnknownCommand;
    FString TargetPlayer;
    FString TargetParty;
    int64 TargetRevision = -1;
    double ServerSnapshotMonotonicSeconds = 0.0;
};
