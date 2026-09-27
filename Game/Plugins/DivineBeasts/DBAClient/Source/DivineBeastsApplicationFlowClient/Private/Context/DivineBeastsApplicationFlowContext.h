#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Flow/DivineBeastsFlowTypes.h"
#include "DivineBeastsApplicationFlowContext.generated.h"

/**
 * UDivineBeastsApplicationFlowContext（神兽联盟应用流程上下文）。
 *
 * 职责：
 * 作为一次 GameInstance（游戏实例）主应用流程的强类型 Payload（载荷），保存真正需要跨节点存在的项目数据。
 * 该对象不是第二套状态机：当前节点、RunId（运行代次）和 NodeGeneration（节点代次）仍完全由
 * GamePlatformApplicationFlow（游戏平台应用流程）持有。
 *
 * 生命周期：
 * Outer 必须是当前 UGameInstance；ApplicationFlow 在活动运行期间保活 Payload。
 * 跨地图后不得依赖旧 UWorld、Actor、Controller 或 Widget，因此本类型禁止保存这些对象强引用。
 *
 * 安全：
 * Endpoint（连接地址）和 TransferTicket（转移票据）仅保存在 Private 非反射字段中，
 * 不进入 Blueprint、ViewState、日志或遥测；票据交给 Session（会话）后立即清空。
 *
 * 性能：
 * 上下文只保存稳定值和少量业务数组，不执行 Tick；节点之间通过同一实例传递数据，
 * 避免重复复制整份玩家资料、角色列表和世界分配结果。
 */
UCLASS(Transient)
class UDivineBeastsApplicationFlowContext final : public UObject
{
    GENERATED_BODY()

public:
    /** 创建/重启主流程前重置业务数据；不会修改平台流程状态。 */
    void ResetForNewRun(bool bInTryAutoLogin);

    bool ShouldTryAutoLogin() const { return bTryAutoLogin; }
    void SetTryAutoLogin(bool bValue) { bTryAutoLogin = bValue; }

    FDivineBeastsPlayerProfile& GetProfile() { return Profile; }
    const FDivineBeastsPlayerProfile& GetProfile() const { return Profile; }

    TArray<FDivineBeastsCharacterSummary>& GetCharacterRoster() { return CharacterRoster; }
    const TArray<FDivineBeastsCharacterSummary>& GetCharacterRoster() const { return CharacterRoster; }

    void SetSelectedCharacter(const FDivineBeastsCharacterSummary& Character);
    const FDivineBeastsCharacterSummary& GetSelectedCharacter() const { return SelectedCharacter; }
    bool HasSelectedCharacter() const { return bHasSelectedCharacter; }

    void SetPendingCreateDraft(const FDivineBeastsCharacterCreateDraft& Draft);
    bool ConsumePendingCreateDraft(FDivineBeastsCharacterCreateDraft& OutDraft);

    void SetPendingSelection(const FDivineBeastsCharacterSummary& Character);
    bool ConsumePendingSelection(FDivineBeastsCharacterSummary& OutCharacter);

    void SetTargetExperience(FName ExperienceId, FString PreferredRegion);
    FName GetTargetExperience() const { return TargetExperienceId; }
    const FString& GetPreferredRegion() const { return PreferredRegion; }

    void SetAssignment(
        const FDivineBeastsWorldAssignmentSummary& InSummary,
        FString InEndpoint,
        FString InTransferTicket);
    const FDivineBeastsWorldAssignmentSummary& GetAssignment() const { return Assignment; }

    /**
     * 一次性取走敏感连接材料。成功后上下文立即清空内部副本，调用方必须继续遵循不日志、不持久化规则。
     */
    bool ConsumeConnectionMaterial(FString& OutEndpoint, FString& OutTransferTicket);

    void ClearConnectionMaterial();

    int32 IncrementRecoveryAttempts() { return ++RecoveryAttempts; }
    void ResetRecoveryAttempts() { RecoveryAttempts = 0; }
    int32 GetRecoveryAttempts() const { return RecoveryAttempts; }

    void SetLoadingObservationId(const FGuid& Value) { LoadingObservationId = Value; }
    const FGuid& GetLoadingObservationId() const { return LoadingObservationId; }

private:
    /** 这些业务值需要跨节点存在，因此由GameInstance作用域上下文统一拥有。 */
    UPROPERTY(Transient)
    FDivineBeastsPlayerProfile Profile;

    UPROPERTY(Transient)
    TArray<FDivineBeastsCharacterSummary> CharacterRoster;

    UPROPERTY(Transient)
    FDivineBeastsCharacterSummary SelectedCharacter;

    UPROPERTY(Transient)
    FDivineBeastsCharacterCreateDraft PendingCreateDraft;

    UPROPERTY(Transient)
    FDivineBeastsCharacterSummary PendingSelection;

    UPROPERTY(Transient)
    FDivineBeastsWorldAssignmentSummary Assignment;

    UPROPERTY(Transient)
    FName TargetExperienceId = NAME_None;

    UPROPERTY(Transient)
    FString PreferredRegion;

    UPROPERTY(Transient)
    FGuid LoadingObservationId;

    int32 RecoveryAttempts = 0;
    bool bTryAutoLogin = true;
    bool bHasSelectedCharacter = false;
    bool bHasPendingCreateDraft = false;
    bool bHasPendingSelection = false;

    /**
     * 敏感连接材料刻意不使用 UPROPERTY，不进入反射/蓝图公开数据模型。
     * FString 本身不需要 GC 追踪；生命周期由本 UObject 严格控制。
     */
    FString PendingEndpoint;
    FString PendingTransferTicket;
};
