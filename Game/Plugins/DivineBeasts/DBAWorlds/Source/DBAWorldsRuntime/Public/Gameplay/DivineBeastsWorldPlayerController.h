#pragma once
#include "Framework/GamePlatformPlayerControllerBase.h"
#include "DivineBeastsWorldPlayerController.generated.h"
class AGamePlatformPlayerStateBase;
class UGamePlatformExperienceComponent;
class AGameStateBase;
class UDivineBeastsCharacterComponent;

/** 第三层双端控制器：事件驱动读取真实体验、Pawn及准备令牌；客户端不提交认证结论。
 * 输入在服务器Active后开放，EndPlay解除本世界全部订阅；不Tick轮询业务状态。 */
UCLASS()
class DBAWORLDSRUNTIME_API ADivineBeastsWorldPlayerController : public AGamePlatformPlayerControllerBase
{
    GENERATED_BODY()
public:
    FSimpleMulticastDelegate OnLocalWorldFactsChanged;
    /** 游戏线程幂等重算，只提交完整准备事实；缺组件/旧令牌时保持输入关闭。 */
    void RefreshLocalWorldFacts();
    /** 当前体验的本地Data租约真正Prepared才返回true；尚未复制/加载则false。 */
    bool AreLocalResourcesPrepared() const;
    /** 当前Controller拥有实际Pawn、复制身份匹配且输入接收器已创建才返回true。 */
    bool IsLocalPawnBound() const;
    /** 只读服务器Active和本地绑定事实，不能把本地键盘绑定当服务器授权。 */
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
    /** 当前本地受控Pawn的角色定义Ready订阅；换Pawn及EndPlay先解绑，不使用永久轮询。 */
    TWeakObjectPtr<UDivineBeastsCharacterComponent> ObservedCharacter;
    bool bRefreshingFacts=false;
};
