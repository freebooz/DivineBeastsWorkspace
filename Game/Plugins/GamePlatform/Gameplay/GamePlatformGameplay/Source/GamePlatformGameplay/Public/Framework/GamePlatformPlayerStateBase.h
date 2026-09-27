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
