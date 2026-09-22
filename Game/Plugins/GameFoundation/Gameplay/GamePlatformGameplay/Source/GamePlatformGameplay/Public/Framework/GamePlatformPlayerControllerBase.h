#pragma once
#include "GameFramework/PlayerController.h"
#include "Types/GamePlatformGameplayReadiness.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformPlayerControllerBase.generated.h"
class AGamePlatformGameModeBase;

/** 拥有者准备报告通道；不接受体验选择、玩家身份、Pawn类或出生坐标等权威参数。 */
UCLASS(NotBlueprintable)
class GAMEPLATFORMGAMEPLAY_API AGamePlatformPlayerControllerBase : public APlayerController
{
    GENERATED_BODY()
public:
    /** 返回当前拥有者令牌；仅供关联准备状态，禁止用作认证凭据。 */
    FGamePlatformPreparationToken GetPreparationToken() const { return PreparationToken; }
    /**
     * 本地组合根报告四项开放前事实；限当前LocalController且实际Pawn匹配。
     * 成功只表示已提交，服务器Active必须读取PlayerState；重复同令牌不重复发送。
     */
    FGamePlatformResult ReportLocalPreparation(const FGamePlatformLocalPreparationFacts& Facts);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    friend class AGamePlatformGameModeBase;
    UPROPERTY(ReplicatedUsing=OnRep_PreparationToken)
    FGamePlatformPreparationToken PreparationToken;
    FGuid LastSubmittedToken;
    UFUNCTION() void OnRep_PreparationToken();
    UFUNCTION(Server, Reliable)
    void ServerReportPrepared(FGamePlatformPreparationToken Token);
    void SetPreparationToken(const FGamePlatformPreparationToken& Value);
};
