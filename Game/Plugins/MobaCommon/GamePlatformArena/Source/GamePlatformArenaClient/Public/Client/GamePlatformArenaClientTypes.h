#pragma once

#include "CoreMinimal.h"
#include "GamePlatformArenaClientTypes.generated.h"

/** FGamePlatformArenaMatchmakingRequest（客户端允许提交的匹配请求字段）。
 *  禁止包含MMR、HiddenRating、TrustScore、Penalty或服务器亲和分等隐藏权威值。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMARENACLIENT_API FGamePlatformArenaMatchmakingRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ArenaModeId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PartyId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PreferredRegion;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ClientRequestId;

    bool IsValid(FString& OutError) const
    {
        if (ArenaModeId.IsNone()) { OutError = TEXT("ArenaModeId不能为空。"); return false; }
        if (ClientRequestId.IsEmpty()) { OutError = TEXT("ClientRequestId不能为空。"); return false; }
        OutError.Reset();
        return true;
    }
};

/** EGamePlatformArenaClientFlowState（竞技客户端流程状态）。 */
UENUM(BlueprintType)
enum class EGamePlatformArenaClientFlowState : uint8
{
    Idle,
    Matchmaking,
    MatchFound,
    Transferring,
    Connecting,
    HeroSelection,
    ReadyCheck,
    InMatch,
    Result,
    Failed
};
