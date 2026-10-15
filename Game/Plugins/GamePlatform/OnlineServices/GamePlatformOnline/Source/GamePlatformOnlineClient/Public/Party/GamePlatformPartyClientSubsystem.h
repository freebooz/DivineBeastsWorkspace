// 平台客户端本人队伍观察：GameInstance作用域的一条可恢复订阅，向多个页面投影只读数据；不拥有队伍权威。
#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/GamePlatformParty.h"
#include "Types/GamePlatformOnlineRequests.h"
#include "GamePlatformPartyClientSubsystem.generated.h"

class UGamePlatformOnlineClientSubsystem;
struct FGamePlatformAuthSnapshot;
DECLARE_MULTICAST_DELEGATE_OneParam(FGamePlatformPartyObservationChanged,const FGamePlatformPartyResult&);

/** 复用Online认证、同源传输、预算和取消；初始快照后只续接服务器等待，不按UI刷新周期读取业务。 */
UCLASS()
class GAMEPLATFORMONLINECLIENT_API UGamePlatformPartyClientSubsystem final:public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    /** 游戏线程只读缓存；bSuccess=false表示状态未知，不能把旧快照当作当前业务真源。 */
    const FGamePlatformPartyResult& GetSnapshot() const {return Snapshot;}
    /** 当前观察绑定的认证代次，页面必须与自身代次一致才接受投影。 */
    FGuid GetAuthenticationGeneration() const {return AuthGeneration;}
    /** 多页面共享事件；订阅者关闭只解除自己的委托，不取消队伍或其他页面订阅。 */
    FGamePlatformPartyObservationChanged& OnChanged(){return Changed;}
    /** 显式重新同步；先废弃旧请求代次，再取消等待，迟到回调不能覆盖新快照。 */
    void Synchronize();
private:
    void AuthenticationChanged(const FGamePlatformAuthSnapshot& Authentication);
    void StopObservation();
    void ContinueObservation(uint64 ExpectedGeneration);
    void CompleteObservation(uint64 ExpectedGeneration,const FGamePlatformPartyResult& Result);
    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> Online;
    FDelegateHandle AuthHandle;
    FTSTicker::FDelegateHandle RetryHandle;
    FGamePlatformOnlineRequestHandle Request;
    FGamePlatformPartyResult Snapshot;
    FGamePlatformPartyObservationChanged Changed;
    FGuid AuthGeneration;
    FString PlayerId;
    FString Cursor;
    uint64 ObservationGeneration=0;
    int32 ConsecutiveFailures=0;
    bool bStopping=false;
};
