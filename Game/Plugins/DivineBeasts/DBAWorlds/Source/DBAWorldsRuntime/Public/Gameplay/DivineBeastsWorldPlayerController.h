#pragma once
#include "Framework/GamePlatformPlayerControllerBase.h"
#include "DivineBeastsWorldPlayerController.generated.h"
class AGamePlatformPlayerStateBase;
class UGamePlatformExperienceComponent;
class AGameStateBase;

/** 第三层双端控制器：事件驱动读取真实体验、Pawn及准备令牌；客户端不提交认证结论。
 * 输入在服务器Active后开放，EndPlay解除本世界全部订阅；不Tick轮询业务状态。 */
UCLASS()
class DBAWORLDSRUNTIME_API ADivineBeastsWorldPlayerController : public AGamePlatformPlayerControllerBase
{
    GENERATED_BODY()
public:
    FSimpleMulticastDelegate OnLocalWorldFactsChanged;
    void RefreshLocalWorldFacts();
    bool AreLocalResourcesPrepared() const;
    bool IsLocalPawnBound() const;
    bool IsWorldGameplayActive() const;
    virtual void OnRep_PlayerState() override;
    virtual void OnRep_Pawn() override;
    virtual void ClientRestart_Implementation(APawn* NewPawn) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void BindGameState(AGameStateBase* State);
    TWeakObjectPtr<AGamePlatformPlayerStateBase> ObservedPlayer;
    TWeakObjectPtr<UGamePlatformExperienceComponent> ObservedExperience;
    bool bRefreshingFacts=false;
};
