#pragma once
// 平台Editor人工审核合同：调用方为编辑器工具与测试，客户端只提供中立文件，不能依赖本模块。
// 报告拥有合同引用、追加修订与证据索引；游戏线程校验，真实人工确认后才保存结论。

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Engine/DataAsset.h"
#include "GamePlatformReviewTypes.generated.h"

/** EGamePlatformReviewStatus（人工验收状态）。 */
UENUM()
enum class EGamePlatformReviewStatus : uint8
{
    NotRun,
    InProgress,
    Passed,
    Failed,
    Blocked
};

/** UGamePlatformReviewCase（游戏平台人工验收测试用例）。 */
UCLASS(BlueprintType)
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformReviewCase : public UDataAsset
{
    GENERATED_BODY()
public:
    /** 稳定用例身份，不能包含路径/脚本或空FName。 */
    UPROPERTY(EditAnywhere) FName CaseId;
    /** 中立分类身份，不认识项目或MOBA规则。 */
    UPROPERTY(EditAnywhere) FName Category;
    /** 中文目的、人工前提、操作、预期和必需类型；文本仅展示，不执行。 */
    UPROPERTY(EditAnywhere) FString Description;
    UPROPERTY(EditAnywhere) TArray<FString> Preconditions;
    UPROPERTY(EditAnywhere) TArray<FString> Steps;
    UPROPERTY(EditAnywhere) FString ExpectedResult;
    UPROPERTY(EditAnywhere) TArray<FString> RequiredEvidence;
    /** 合同正版本，变化后旧报告不计当前通过。 */
    UPROPERTY(EditAnywhere) int32 CaseVersion = 1;
    /** 可选中立功能/案例身份；演示编目必须填，旧普通用例可为空但不冒充演示覆盖。 */
    UPROPERTY(EditAnywhere) FName FeatureId;
    UPROPERTY(EditAnywhere) FName ScenarioId;
    /** 结构/文本/证据类型完整性；失败给中文原因，不加载或写文件。 */
    bool IsCaseComplete(FString& OutError) const;
    /** 当前完整合同SHA256；算法不可用返回空，不能把版本号当合同内容未变证明。 */
    FString GetContractFingerprint() const;
};

/** 已显式确认的人工修订值；保留前结论和证据，不修改原始运行结果。 */
USTRUCT()
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformReviewRevision
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere) int32 Revision = 0;
    UPROPERTY(VisibleAnywhere) EGamePlatformReviewStatus Status = EGamePlatformReviewStatus::NotRun;
    UPROPERTY(VisibleAnywhere) FString Reviewer;
    UPROPERTY(VisibleAnywhere) FDateTime Date;
    UPROPERTY(VisibleAnywhere) FString Comment;
    UPROPERTY(VisibleAnywhere) TArray<FString> Evidence;
    /** 固定RunId下真实落盘修订路径，不能覆盖旧文件。 */
    UPROPERTY(VisibleAnywhere) FString SavedPath;
};

/** UGamePlatformReviewReport（游戏平台人工验收报告对象）。 */
UCLASS(BlueprintType)
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformReviewReport : public UObject
{
    GENERATED_BODY()
public:
    /** 运行GUID、用例和人工状态；没有人工动作默认NotRun。 */
    UPROPERTY(VisibleAnywhere) FString RunId;
    UPROPERTY(VisibleAnywhere) FName CaseId;
    UPROPERTY(VisibleAnywhere) EGamePlatformReviewStatus Status = EGamePlatformReviewStatus::NotRun;
    UPROPERTY(VisibleAnywhere) FString Reviewer;
    UPROPERTY(VisibleAnywhere) FDateTime ReviewDate;
    UPROPERTY(VisibleAnywhere) FString Comment;
    UPROPERTY(VisibleAnywhere) TArray<FString> Evidence;
    UPROPERTY(VisibleAnywhere) TMap<FString, double> Metrics;
    UPROPERTY(VisibleAnywhere) FString BuildVersion;
    UPROPERTY(VisibleAnywhere) FString ContentRevision;
    /** 创建时冻结的合同版本和指纹，资产内容变化后必须重新建立报告。 */
    UPROPERTY(VisibleAnywhere) int32 CaseVersion = 0;
    UPROPERTY(VisibleAnywhere) FString CaseFingerprint;
    /** 完整用例对象由报告持有，仅在Editor，不进入客户端公共头。 */
    UPROPERTY(Transient) TObjectPtr<UGamePlatformReviewCase> BoundCase;
    /** 独立运行索引解析出的中立身份及三类版本；来源均是已核验文件。 */
    UPROPERTY(VisibleAnywhere) FName FeatureId;
    UPROPERTY(VisibleAnywhere) FName ScenarioId;
    UPROPERTY(VisibleAnywhere) FString SourceFingerprint;
    UPROPERTY(VisibleAnywhere) FString ContentFingerprint;
    UPROPERTY(VisibleAnywhere) FString RunEvidenceIndex;
    /** 真正终态/用户操作证据和匹配二进制证明，未导入完整证明均false。 */
    UPROPERTY(VisibleAnywhere) bool bRuntimeCompleted = false;
    UPROPERTY(VisibleAnywhere) bool bBinaryVerified = false;
    /** 导入时对照的当前源/内容指纹，不凭历史Passed推断。 */
    UPROPERTY(VisibleAnywhere) FString CurrentSourceFingerprint;
    UPROPERTY(VisibleAnywhere) FString CurrentContentFingerprint;
    /** 原始运行证据摘要，人工签收前重读，防止导入后文件变化。 */
    UPROPERTY(VisibleAnywhere) FString RunEvidenceHash;
    /** 独立无构建运行器证明及摘要；人工签收前重读，导入后修改证明不能沿用旧true。 */
    UPROPERTY(VisibleAnywhere) FString BinaryVerificationPath;
    UPROPERTY(VisibleAnywhere) FString BinaryVerificationHash;
    /** 已导入文件的类型:path→SHA256；签收重新核验，外加证据也检查实际文件。 */
    UPROPERTY(VisibleAnywhere) TMap<FString, FString> ImportedEvidenceHashes;
    /** 追加修订历史；Status等旧字段投影最新修订，保持已发布反射名。 */
    UPROPERTY(VisibleAnywhere) TArray<FGamePlatformReviewRevision> Revisions;
};

/** UGamePlatformReviewSubsystem（游戏平台人工验收子系统）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformReviewSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()
public:
    /** 完整用例和规范RunGUID才创建待审；不自动开始/签收，不修改旧接口签名。 */
    UGamePlatformReviewReport* CreatePendingReport(
        UGamePlatformReviewCase* ReviewCase,
        const FString& RunId,
        const FString& BuildVersion,
        const FString& ContentRevision);
    /** 只读绑定真实运行索引与当前版本；失败保留旧报告，不能导入人工Reviewer/Passed。 */
    bool BindRunEvidence(UGamePlatformReviewReport* Report, const FString& IndexPath,
        const FString& CurrentSourceFingerprint, const FString& CurrentContentFingerprint, FString& OutError);
    /** 检查完整合同、当前版本、真实终态/二进制及必需证据；不能执行人工决定。 */
    bool ValidateReviewEvidence(const UGamePlatformReviewCase& ReviewCase, const UGamePlatformReviewReport& Report, FString& OutError) const;
    /** 显式人工确认、具名审核者和合法目标状态后追加修订；写盘失败不改内存结论。 */
    bool ApplyHumanDecision(
        UGamePlatformReviewReport* Report,
        EGamePlatformReviewStatus NewStatus,
        const FString& Reviewer,
        const FString& Comment,
        const TArray<FString>& Evidence,
        bool bExplicitHumanConfirmation,
        FString& OutError);
};
