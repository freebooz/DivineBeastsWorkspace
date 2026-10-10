// 第三层服务器私有适配：真实准入PlayerId读取PlayerData已选择角色，再把身份绑定到平台生成的Pawn。
// 不复制账号/HTTP响应，不接收客户端Hero自报；请求、生命周期委托和冻结角色快照归当前服务器世界。
#pragma once
#include "CoreMinimal.h"
#include "Server/GamePlatformServerAdmissionSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "TimerManager.h"
class ADivineBeastsWorldGameMode;
class AGamePlatformPlayerStateBase;
class UDivineBeastsCharacterComponent;

/** 后端只读响应解码；必须同时核对玩家、已拥有的选中角色、Active状态及项目Catalog。失败不返回默认英雄。 */
namespace DivineBeasts::WorldCharacterAdmission
{
    bool ParseSelectedCharacter(const FString& Json, const FString& VerifiedPlayerId, FString& OutCharacterId);
    bool ParseSelectedHero(const FString& Json, const FString& SelectedCharacterId, FName& OutHeroId);
    /** 仅消费当前实际Pawn的角色Ready，不能把AuthorityBind返回的受理结论当作初始化成功。 */
    bool IsPawnReadyForActivation(const APlayerController& Controller);
}

/** 游戏线程世界作用域桥；仅Bootstrap配置。共享指针负责原生请求寿命，弱引用拒绝旧世界/连接回调。 */
class FDivineBeastsWorldCharacterAdmission final : public TSharedFromThis<FDivineBeastsWorldCharacterAdmission>
{
public:
    FDivineBeastsWorldCharacterAdmission(ADivineBeastsWorldGameMode& Mode, UGamePlatformServerAdmissionSubsystem& Admission);
    ~FDivineBeastsWorldCharacterAdmission();
    /** MakeShared之后仅调用一次；原生准入订阅归本桥，Close精确解绑。 */
    void ObserveAdmissions();
    /** 重复同准入幂等；新准入替换前撤销旧请求。准入撤销同步撤掉本桥订阅并交平台清理Pawn。 */
    void HandleAdmission(const APlayerController* Controller, const FGamePlatformServerVerifiedAdmission& Verified, bool bAccepted);
    /** 先关闭新回调再取消HTTP/解绑事件；不停止其他世界的资源或请求。 */
    void Close();
private:
    struct FConnection
    {
        FGamePlatformServerVerifiedAdmission Verified;
        FString CharacterId;
        FName HeroId;
        FGuid OperationId;
        TWeakObjectPtr<AGamePlatformPlayerStateBase> PlayerState;
        TWeakObjectPtr<APawn> BoundPawn;
        FDelegateHandle LifecycleHandle;
        /** 必要定义初始化终态订阅与十秒有界等待；不把身份受理当作Ready。 */
        TWeakObjectPtr<UDivineBeastsCharacterComponent> CharacterIdentity;
        FDelegateHandle ReadinessHandle;
        FTimerHandle InitializationTimeout;
        /** 延迟失败撤销也是本连接的资源，关闭时显式清理，不保留已退休世界闭包。 */
        FTimerHandle FailureTimer;
        /** 每次绑定后继Pawn更新；旧Ready/超时/失败闭包不能凭同连接身份撤销后继Avatar。 */
        FGuid PawnOperationId;
        bool bWasCharacterReady = false;
        bool bFailureScheduled = false;
        TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request;
    };
    /** 每次请求完成/外部通知后重验真实准入、Controller世界与操作身份；结构相同不等于授权仍有效。 */
    bool IsCurrent(APlayerController* Controller, const TSharedPtr<FConnection>& Connection) const;
    void ReadProfile(APlayerController& Controller, const TSharedPtr<FConnection>& Connection);
    void ReadRoster(APlayerController& Controller, const TSharedPtr<FConnection>& Connection);
    void RequestJson(APlayerController& Controller, const TSharedPtr<FConnection>& Connection, const FString& Route,
        TFunction<void(FString)> Completed);
    void PublishAdmission(APlayerController& Controller, const TSharedPtr<FConnection>& Connection);
    void BindSpawnedPawn(APlayerController& Controller, const TSharedPtr<FConnection>& Connection);
    /** 成功清除本Pawn截止，失败/取消或已Ready后失活撤销本连接；不会操作后继Pawn的计时器。 */
    void ObserveCharacterInitialization(APlayerController& Controller, const TSharedPtr<FConnection>& Connection);
    /** 错误只记固定诊断码；下一游戏线程时隙撤销避免在平台Publish的同步栈中删除其玩家记录。 */
    void Fail(APlayerController& Controller, const TSharedPtr<FConnection>& Connection, FName Code);
    void ReleaseConnection(const TSharedPtr<FConnection>& Connection);
    TWeakObjectPtr<ADivineBeastsWorldGameMode> Mode;
    TWeakObjectPtr<UGamePlatformServerAdmissionSubsystem> Admission;
    TMap<TWeakObjectPtr<APlayerController>, TSharedPtr<FConnection>> Connections;
    FString PlayerDataBaseUrl;
    FDelegateHandle AdmissionChangedHandle;
    bool bClosed = false;
};
