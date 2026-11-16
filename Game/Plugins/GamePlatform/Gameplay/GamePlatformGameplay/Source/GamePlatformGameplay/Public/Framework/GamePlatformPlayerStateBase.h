// 平台双端玩家公开状态：复用引擎姓名复制和平台生命周期；不保存项目角色档案或客户端UI。
#pragma once
#include "GameFramework/PlayerState.h"
#include "Types/GamePlatformPlayerLifecycle.h"
#include "GamePlatformPlayerStateBase.generated.h"
class AGamePlatformGameModeBase;

/** 公开玩家生命周期唯一复制载体；内部认证映射不进入本对象。 */
UCLASS(NotBlueprintable)
class GAMEPLATFORMGAMEPLAY_API AGamePlatformPlayerStateBase : public APlayerState
{
    GENERATED_BODY()
public:
    /** 当前复制事实变化事件；只通知消费者重新读取快照，不维护另一套状态机。 */
    FSimpleMulticastDelegate OnLifecycleChanged;
    /** 引擎公开姓名到达/变化后的GT通知；本地UI订阅后读一次GetPlayerName，失活须解绑。 */
    FSimpleMulticastDelegate OnPublicPlayerNameChanged;
    /** 复用APlayerState既有复制字段；客户端及Listen/Standalone经原生RepNotify发布姓名事件。 */
    virtual void OnRep_PlayerName() override;
    /** 当前公开值快照；Pawn可能尚未网络解析，观察者应再次采样。 */
    FGamePlatformPlayerLifecycleSnapshot GetLifecycleSnapshot() const { return Snapshot; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    friend class AGamePlatformGameModeBase;
    UPROPERTY(ReplicatedUsing=OnRep_Snapshot)
    FGamePlatformPlayerLifecycleSnapshot Snapshot;
    UFUNCTION() void OnRep_Snapshot();
    void Publish(const FGamePlatformPlayerLifecycleSnapshot& Value);
};
