#pragma once

#include "CoreMinimal.h"
#include "GamePlatformArenaTypes.generated.h"

/** EGamePlatformArenaMatchPhase（竞技比赛阶段）。阶段只能由服务器权威推进。 */
UENUM(BlueprintType)
enum class EGamePlatformArenaMatchPhase : uint8
{
    Uninitialized,
    WaitingAssignment,
    Preparing,
    WaitingPlayers,
    HeroSelection,
    ReadyCheck,
    Countdown,
    InProgress,
    Ending,
    ResultPending,
    Completed,
    Aborted,
    Failed
};

/** EGamePlatformArenaMatchEndReason（比赛结束原因）。 */
UENUM(BlueprintType)
enum class EGamePlatformArenaMatchEndReason : uint8
{
    None,
    ScoreLimit,
    Elimination,
    Objective,
    TimeLimit,
    PlayerForfeit,
    TeamForfeit,
    ReconnectTimeout,
    AdministrativeAbort,
    ServerFailure
};

/** EGamePlatformArenaConnectionState（参赛者连接状态）。 */
UENUM(BlueprintType)
enum class EGamePlatformArenaConnectionState : uint8
{
    PendingAdmission,
    Connected,
    Disconnected,
    Reconnecting,
    TimedOut
};

/** EGamePlatformArenaForfeitState（弃权状态）。客户端只能请求，最终状态由服务器决定。 */
UENUM(BlueprintType)
enum class EGamePlatformArenaForfeitState : uint8
{
    None,
    Requested,
    Accepted
};

/** EGamePlatformArenaTrustedEventType（竞技可信事件类型）。 */
UENUM(BlueprintType)
enum class EGamePlatformArenaTrustedEventType : uint8
{
    Death,
    Assist,
    Objective,
    Forfeit,
    TimeExpired
};

/** EGamePlatformArenaWinDecisionType（胜负判定结果）。 */
UENUM(BlueprintType)
enum class EGamePlatformArenaWinDecisionType : uint8
{
    Continue,
    TeamWin,
    Draw,
    Overtime,
    Abort
};

/** FGamePlatformArenaRosterSlot（比赛名单槽位）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaRosterSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString PlayerId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString CharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName TeamId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString PartyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 SlotIndex = INDEX_NONE;

    bool IsValid() const { return !PlayerId.IsEmpty() && !CharacterId.IsEmpty() && !TeamId.IsNone() && SlotIndex >= 0; }
};

/** FGamePlatformArenaAssignment（MainArena服务器最终比赛分配）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaAssignment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString MatchId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ArenaModeId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MapId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ServerRole = TEXT("GameServer.Role.MainArena");
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ExperienceId = TEXT("Experience.MainArena.Main");
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ProjectRuleRevision = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString ContentRevision;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 HeroCatalogRevision = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString GameServerId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TeamSize = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TotalPlayers = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FGamePlatformArenaRosterSlot> Roster;
};

/** FGamePlatformArenaTransferTicketClaims（已由后端验证后的转服票据声明；不含签名/Nonce原文）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaTransferTicketClaims
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString TicketId;
    UPROPERTY(BlueprintReadOnly) FString PlayerId;
    UPROPERTY(BlueprintReadOnly) FString CharacterId;
    UPROPERTY(BlueprintReadOnly) FString SessionId;
    UPROPERTY(BlueprintReadOnly) FString MatchId;
    UPROPERTY(BlueprintReadOnly) FString DestinationServerId;
    UPROPERTY(BlueprintReadOnly) FDateTime ExpiresAtUtc;
    UPROPERTY(BlueprintReadOnly) bool bConsumed = false;
};

/** FGamePlatformArenaTeamState（复制给客户端的队伍公共状态）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaTeamState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName TeamId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Score = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ObjectiveScore = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Revision = 0;
};

/** FGamePlatformArenaObjectiveState（目标公共状态）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaObjectiveState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ObjectiveId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName OwningTeamId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Value = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Revision = 0;
};

/** FGamePlatformArenaPlayerResult（玩家结算事实）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaPlayerResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString PlayerId;
    UPROPERTY(BlueprintReadOnly) FName TeamId;
    UPROPERTY(BlueprintReadOnly) FString CharacterId;
    UPROPERTY(BlueprintReadOnly) int32 Kills = 0;
    UPROPERTY(BlueprintReadOnly) int32 Deaths = 0;
    UPROPERTY(BlueprintReadOnly) int32 Assists = 0;
    UPROPERTY(BlueprintReadOnly) int32 Score = 0;
};

/** FGamePlatformArenaTeamResult（队伍结算事实）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaTeamResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName TeamId;
    UPROPERTY(BlueprintReadOnly) int32 Score = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FGamePlatformArenaPlayerResult> Players;
};

/** FGamePlatformArenaMatchResult（权威比赛结算；长期MMR/奖励不在此计算）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaMatchResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString MatchId;
    UPROPERTY(BlueprintReadOnly) FName ArenaModeId;
    UPROPERTY(BlueprintReadOnly) FString GameServerId;
    UPROPERTY(BlueprintReadOnly) FDateTime StartedAtUtc;
    UPROPERTY(BlueprintReadOnly) FDateTime EndedAtUtc;
    UPROPERTY(BlueprintReadOnly) FName WinningTeamId;
    UPROPERTY(BlueprintReadOnly) EGamePlatformArenaMatchEndReason EndReason = EGamePlatformArenaMatchEndReason::None;
    UPROPERTY(BlueprintReadOnly) TArray<FGamePlatformArenaTeamResult> Teams;
    UPROPERTY(BlueprintReadOnly) int32 EndRevision = 0;
};

/** FGamePlatformArenaResultSummary（GameState只复制必要的公开结算摘要）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaResultSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName WinningTeamId;
    UPROPERTY(BlueprintReadOnly) EGamePlatformArenaMatchEndReason EndReason = EGamePlatformArenaMatchEndReason::None;
    UPROPERTY(BlueprintReadOnly) int32 EndRevision = 0;
};

/** FGamePlatformArenaTrustedEvent（由Combat/Objective等权威系统产生的中立事实）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaTrustedEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString EventId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EGamePlatformArenaTrustedEventType EventType = EGamePlatformArenaTrustedEventType::Death;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString PlayerId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString RelatedPlayerId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName TeamId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Value = 0;
};

/** FGamePlatformArenaScoreDelta（评分策略输出）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaScoreDelta
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName TeamId;
    UPROPERTY(BlueprintReadOnly) FString PlayerId;
    UPROPERTY(BlueprintReadOnly) int32 TeamScoreDelta = 0;
    UPROPERTY(BlueprintReadOnly) int32 PlayerScoreDelta = 0;
    UPROPERTY(BlueprintReadOnly) int32 ObjectiveScoreDelta = 0;
};

/** FGamePlatformArenaWinDecision（胜负策略输出）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBACORE_API FGamePlatformArenaWinDecision
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EGamePlatformArenaWinDecisionType Decision = EGamePlatformArenaWinDecisionType::Continue;
    UPROPERTY(BlueprintReadOnly) FName WinningTeamId;
    UPROPERTY(BlueprintReadOnly) EGamePlatformArenaMatchEndReason EndReason = EGamePlatformArenaMatchEndReason::None;
};
