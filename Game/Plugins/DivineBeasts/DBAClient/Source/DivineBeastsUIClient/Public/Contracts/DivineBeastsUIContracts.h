#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DivineBeastsUIContracts.generated.h"

/** EDivineBeastsUIPageState（神兽联盟UI页面状态）。 */
UENUM(BlueprintType)
enum class EDivineBeastsUIPageState : uint8
{
    Loading,
    Ready,
    Empty,
    Disabled,
    Error,
    Retry,
    NoPermission,
    Submitting
};

/** EDivineBeastsUIResultCommitState（竞技赛后结果提交状态）。 */
UENUM(BlueprintType)
enum class EDivineBeastsUIResultCommitState : uint8
{
    None,
    Pending,
    Committed,
    Failed
};

/** EDivineBeastsUICommandType（面向UI命令类型）。 */
enum class EDivineBeastsUICommandType : uint8
{
    TryAutoLogin,
    Login,
    Refresh,
    Retry,
    CreateCharacter,
    SelectPersistentCharacter,
    RequestWorld,
    Logout,
    TrainingReset,
    StartMatchmaking,
    CancelMatchmaking,
    ArenaSelectHero,
    ArenaReady,
    PostMatchReturnToWorld
};

/** FDivineBeastsUICharacterItem（持久角色列表UI投影）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUICharacterItem
{
    GENERATED_BODY()

    /** 内部档案ID，不应直接作为Shipping用户可见文案。 */
    UPROPERTY(BlueprintReadOnly) FString CharacterId;
    UPROPERTY(BlueprintReadOnly) FName HeroDefinitionId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FText DisplayName;
    UPROPERTY(BlueprintReadOnly) FName AppearanceProfileId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName Status = NAME_None;
    UPROPERTY(BlueprintReadOnly) bool bSelected = false;
    UPROPERTY(BlueprintReadOnly) bool bEnabled = true;
    UPROPERTY(BlueprintReadOnly) FText DisabledReason;
};

/** FDivineBeastsUICreateHeroItem（角色创建页可选英雄只读项）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUICreateHeroItem
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName HeroDefinitionId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName DisplayNameKey = NAME_None;
};

/** FDivineBeastsUILoadingProjection（加载UI只读投影）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUILoadingProjection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bIsLoading = false;
    UPROPERTY(BlueprintReadOnly) int32 ActiveTokenCount = 0;
    UPROPERTY(BlueprintReadOnly) FText Stage;
    /** 小于0表示真实进度未知，UI不得伪造0~100。 */
    UPROPERTY(BlueprintReadOnly) float Progress = -1.0f;
};

/** FDivineBeastsUIWorldProjection（开放世界/新手村HUD只读投影）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUIWorldProjection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName ExperienceId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName WorldId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName RegionId = NAME_None;

    UPROPERTY(BlueprintReadOnly) float Health = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MaxHealth = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Shield = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MaxShield = 0.0f;

    /** 当前工作树尚无统一项目HUD模型时可为空。 */
    UPROPERTY(BlueprintReadOnly) TArray<FName> AbilityIds;
    UPROPERTY(BlueprintReadOnly) TArray<FName> StatusIds;
    UPROPERTY(BlueprintReadOnly) TArray<FName> QuestObjectiveIds;
    UPROPERTY(BlueprintReadOnly) FName InteractionPromptId = NAME_None;
};

/** FDivineBeastsUIArenaTeam（竞技队伍只读投影）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUIArenaTeam
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName TeamId = NAME_None;
    UPROPERTY(BlueprintReadOnly) int32 Score = 0;
    UPROPERTY(BlueprintReadOnly) int32 ObjectiveScore = 0;
    UPROPERTY(BlueprintReadOnly) int32 Revision = 0;
};

/** FDivineBeastsUIArenaPlayerRow（竞技记分板UI行）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUIArenaPlayerRow
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString PlayerId;
    UPROPERTY(BlueprintReadOnly) FString CharacterId;
    UPROPERTY(BlueprintReadOnly) FName TeamId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName HeroDefinitionId = NAME_None;
    UPROPERTY(BlueprintReadOnly) int32 Kills = 0;
    UPROPERTY(BlueprintReadOnly) int32 Deaths = 0;
    UPROPERTY(BlueprintReadOnly) int32 Assists = 0;
    UPROPERTY(BlueprintReadOnly) int32 Score = 0;
    UPROPERTY(BlueprintReadOnly) bool bReady = false;
};

/** FDivineBeastsUIArenaProjection（竞技UI只读投影）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUIArenaProjection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName ArenaModeId = NAME_None;
    /** 项目允许展示的竞技模式ID；不代表匹配服务当前可提交。 */
    UPROPERTY(BlueprintReadOnly) TArray<FName> AvailableArenaModeIds;
    UPROPERTY(BlueprintReadOnly) FName MatchPhaseId = NAME_None;
    UPROPERTY(BlueprintReadOnly) double RemainingPhaseSeconds = 0.0;
    UPROPERTY(BlueprintReadOnly) TArray<FDivineBeastsUIArenaTeam> Teams;
    UPROPERTY(BlueprintReadOnly) TArray<FDivineBeastsUIArenaPlayerRow> Scoreboard;
    UPROPERTY(BlueprintReadOnly) FName WinningTeamId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName EndReasonId = NAME_None;
    UPROPERTY(BlueprintReadOnly) EDivineBeastsUIResultCommitState ResultCommitState =
        EDivineBeastsUIResultCommitState::None;

    UPROPERTY(BlueprintReadOnly) bool bMatchmakingTransportAvailable = false;
    UPROPERTY(BlueprintReadOnly) bool bHeroSelectionCommandAvailable = false;
};

/** FDivineBeastsUIViewState（项目UI统一只读视图状态）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUIViewState
{
    GENERATED_BODY()

    /** Authority owner运行代次；改变时允许Revision重新从较小值开始。 */
    UPROPERTY(BlueprintReadOnly) FGuid StateRunId;
    UPROPERTY(BlueprintReadOnly) int64 Revision = 0;
    UPROPERTY(BlueprintReadOnly) FName CurrentStep = NAME_None;
    UPROPERTY(BlueprintReadOnly) EDivineBeastsUIPageState PageState =
        EDivineBeastsUIPageState::Loading;
    UPROPERTY(BlueprintReadOnly) bool bBusy = false;
    UPROPERTY(BlueprintReadOnly) FName ErrorCode = NAME_None;
    UPROPERTY(BlueprintReadOnly) FText ErrorText;

    UPROPERTY(BlueprintReadOnly) bool bAuthenticated = false;
    UPROPERTY(BlueprintReadOnly) bool bMaintenance = false;

    UPROPERTY(BlueprintReadOnly) TArray<FDivineBeastsUICharacterItem> Characters;
    UPROPERTY(BlueprintReadOnly) TArray<FDivineBeastsUICreateHeroItem> CreateHeroOptions;
    UPROPERTY(BlueprintReadOnly) FString SelectedCharacterId;
    UPROPERTY(BlueprintReadOnly) FName SelectedHeroDefinitionId = NAME_None;

    UPROPERTY(BlueprintReadOnly) FDivineBeastsUILoadingProjection Loading;
    UPROPERTY(BlueprintReadOnly) FDivineBeastsUIWorldProjection World;
    UPROPERTY(BlueprintReadOnly) FDivineBeastsUIArenaProjection Arena;

    /** UI可提交的命令集合；未知命令必须默认拒绝。 */
    UPROPERTY(BlueprintReadOnly) TArray<FName> AllowedCommands;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsUIViewStateChangedNative,
    const FDivineBeastsUIViewState&);

/** UI命令请求，不是持久化对象；Password只允许瞬时传递。 */
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUICommand
{
    FGuid RequestId;
    EDivineBeastsUICommandType Type = EDivineBeastsUICommandType::Refresh;
    int32 PageGeneration = 0;
    int64 ExpectedRevision = 0;

    FString LoginName;
    FString Password;

    FName HeroDefinitionId = NAME_None;
    FString CharacterName;
    TMap<FString, FString> AppearanceSelection;
    FString CharacterId;

    FName DesiredExperienceId = NAME_None;
    FString PreferredRegion;

    FName ArenaModeId = NAME_None;
    FString PartyId;

    bool IsValid(FString& OutError) const;
};

/** FDivineBeastsUICommandResult（UI命令即时提交结果）。 */
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUICommandResult
{
    FGuid RequestId;
    bool bAccepted = false;
    FName ErrorCode = NAME_None;
};

UINTERFACE()
class DIVINEBEASTSUICLIENT_API UDivineBeastsUIQuerySource : public UInterface
{
    GENERATED_BODY()
};

/** IDivineBeastsUIQuerySource（项目UI查询源接口）。 */
class DIVINEBEASTSUICLIENT_API IDivineBeastsUIQuerySource
{
    GENERATED_BODY()

public:
    virtual FDivineBeastsUIViewState GetUIViewState() const = 0;
    virtual FDivineBeastsUIViewStateChangedNative& OnUIViewStateChanged() = 0;
};

UINTERFACE()
class DIVINEBEASTSUICLIENT_API UDivineBeastsUICommandPort : public UInterface
{
    GENERATED_BODY()
};

/** IDivineBeastsUICommandPort（项目UI命令端口接口）。 */
class DIVINEBEASTSUICLIENT_API IDivineBeastsUICommandPort
{
    GENERATED_BODY()

public:
    virtual void SubmitUICommand(
        const FDivineBeastsUICommand& Command,
        TFunction<void(const FDivineBeastsUICommandResult&)> Completion) = 0;

    virtual bool CancelUICommand(const FGuid& RequestId) = 0;
};
