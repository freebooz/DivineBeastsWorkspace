#pragma once
#include "GameFramework/GameModeBase.h"
#include "Interfaces/IGamePlatformGameplayAdmissionSink.h"
#include "Interfaces/IGamePlatformSpawnPolicy.h"
#include "Types/GamePlatformGameplayReadiness.h"
#include "GamePlatformGameModeBase.generated.h"

class UGamePlatformExperienceComponent;
struct FGamePlatformGameplayServerRuntime;

/**
 * 服务器规则与玩家登记的唯一所有者。所有权威API仅游戏线程；公开读取走GameState/PlayerState。
 * 不允许蓝图覆盖出生门禁；项目可用原生有限派生装配可信适配器，保留原生PreLogin链。
 */
UCLASS(NotBlueprintable)
class GAMEPLATFORMGAMEPLAY_API AGamePlatformGameModeBase : public AGameModeBase, public IGamePlatformGameplayAdmissionSink
{
    GENERATED_BODY()
public:
    AGamePlatformGameModeBase();
    AGamePlatformGameModeBase(FVTableHelper& Helper);
    virtual ~AGamePlatformGameModeBase() override;
    virtual FGamePlatformGameplayRegistration RegisterAdmissionAuthority(TWeakObjectPtr<UObject> Owner,
        TSharedRef<IGamePlatformGameplayAdmissionAuthority> Authority, FGamePlatformResult& OutResult) override;
    virtual bool UnregisterAdmissionAuthority(const FGamePlatformGameplayRegistration& Registration) override;
    virtual FGamePlatformResult SubmitVerifiedAdmission(APlayerController& Controller,
        const FGamePlatformVerifiedPlayerContext& Context) override;
    virtual FGamePlatformResult RevokeVerifiedAdmission(APlayerController& Controller,
        const FGamePlatformAdmissionRevocation& Revocation) override;
    virtual FGamePlatformSpawnEligibility EvaluatePlayerStartEligibility(APlayerController& Controller) const override;
    virtual FGamePlatformResult RequestServerRestart(APlayerController& Controller) override;
    /** 在本世界注册唯一出生策略键；内建PlayerStart占用该保留键。弱Owner失效停止新使用。 */
    FGamePlatformGameplayRegistration RegisterSpawnPolicy(FName PolicyId, TWeakObjectPtr<UObject> Owner,
        TSharedRef<IGamePlatformSpawnPolicy> Policy, FGamePlatformResult& OutResult);
    /** 撤销后等待玩家失败并释放自己的占位；已完成出生不依赖已归还的候选。 */
    bool UnregisterSpawnPolicy(const FGamePlatformGameplayRegistration& Registration);
    /** 准备RPC唯一接收路径；不信任客户端布尔值，重新核对当前连接、拥有关系、代次及截止。 */
    FGamePlatformResult AcceptPreparation(APlayerController& Controller, const FGamePlatformPreparationToken& Token);
    /** 每条玩法权威命令在执行前调用，资格撤销即返回false，不依赖客户端禁键。 */
    bool IsPlayerGameplayActive(const APlayerController& Controller) const;
    /** 体验结束或失败时调用；先失效令牌和记录，再撤销控制与Pawn，最后允许组件释放类资源。 */
    void DrainPlayers(FName Reason);
    /** 当前世界已存在的体验组件，无有效GameState时为空。 */
    UGamePlatformExperienceComponent* GetExperienceComponent() const;

    // 以下UE默认入口全部汇入同一门禁；直接传入的外部出生点或坐标不具有批准效力。
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override final;
    virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override final;
    virtual void RestartPlayer(AController* NewPlayer) override final;
    virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override final;
    virtual void RestartPlayerAtTransform(AController* NewPlayer, const FTransform& SpawnTransform) override final;
    virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override final;
    virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override final;
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* Controller) override final;
    virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override final;
    virtual void Logout(AController* Exiting) override;
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TUniquePtr<FGamePlatformGameplayServerRuntime> Runtime;
    void AdvancePlayer(APlayerController& Controller);
    void TrySpawn(APlayerController& Controller);
    void RemovePlayer(APlayerController& Controller, FName Reason, bool bFailed);
    void PublishPlayer(APlayerController& Controller, EGamePlatformPlayerStage Stage, FName Code = NAME_None);
    FGamePlatformResult ValidateAdmission(const APlayerController& Controller) const;
};
