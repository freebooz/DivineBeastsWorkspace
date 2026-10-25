#pragma once
#include "GameFramework/GameStateBase.h"
#include "GamePlatformGameStateBase.generated.h"
class UGamePlatformExperienceComponent;
/** 引擎复制时钟完成校正后的通知；游戏线程、秒，仅供表现校准，不传递玩法权威。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FGamePlatformServerTimeSynchronized, double);

/** 服务器体验快照的唯一全员复制载体；不拥有账号、票据或第二套体验状态机。 */
UCLASS(NotBlueprintable)
class GAMEPLATFORMGAMEPLAY_API AGamePlatformGameStateBase : public AGameStateBase
{
    GENERATED_BODY()
public:
    AGamePlatformGameStateBase();
    /** 当前GameState创建并持有的组件；游戏线程使用，世界结束后禁止缓存。 */
    UGamePlatformExperienceComponent* GetExperienceComponent() const { return Experience; }
    /** 本客户端是否收到过真实时钟复制；服务器/单机直接使用本世界时间，视为就绪。 */
    bool HasSynchronizedServerTime() const;
    /** 订阅本GameState的时钟校正；调用方于失活/换World成对解绑。无Tick或额外网络消息。 */
    FDelegateHandle AddServerTimeSynchronizedHandler(FGamePlatformServerTimeSynchronized::FDelegate Handler);
    void RemoveServerTimeSynchronizedHandler(FDelegateHandle Handle);
protected:
    virtual void OnRep_ReplicatedWorldTimeSecondsDouble() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    FGamePlatformServerTimeSynchronized ServerTimeSynchronized;
    bool bHasReceivedServerTime = false;
    UPROPERTY(VisibleAnywhere, Category="GamePlatform|Gameplay")
    TObjectPtr<UGamePlatformExperienceComponent> Experience;
};
