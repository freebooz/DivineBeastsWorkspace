// SSLM项目演示运行值合同；开发客户端/编辑器，目录、世界、适配器和证据共同消费。
// 身份/结果拥有值副本；世界仅弱引用，资源和计时器归执行世界，不能跨实例操作。
#pragma once
#include "CoreMinimal.h"
#include "Contracts/DivineBeastsDemoCatalogTypes.h"
#include "DivineBeastsDemoRunTypes.generated.h"

/** 单次案例状态；准备受理不等于运行，清理失败必须进入Failed。 */
UENUM(BlueprintType)
enum class EDivineBeastsDemoRunState : uint8
{
    Idle, Preparing, Running, Stopping, Completed, Failed, Cancelled
};

/** 完整世界运行身份；代次只用于本世界内拒绝旧回调，不代替唯一GUID。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoRunToken
{
    GENERATED_BODY()
    /** 每次运行新生成的不可复用身份。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FGuid RunId;
    /** 本世界作用域身份；同一PIE的另一个世界也不同。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FGuid WorldId;
    /** 世界内正代次，不暴露不受支持的Blueprint uint64数值引脚。 */
    UPROPERTY() uint64 Generation = 0;
    /** 只检查值形状；当前性由世界私有状态机判断。 */
    bool IsValid() const { return RunId.IsValid() && WorldId.IsValid() && Generation > 0; }
    /** 三项都相等才同一次运行，禁止仅比较RunId或整数槽位。 */
    bool operator==(const FDivineBeastsDemoRunToken& Other) const
    { return RunId == Other.RunId && WorldId == Other.WorldId && Generation == Other.Generation; }
};

/** 适配注册句柄；拥有者为单一游戏实例，撤销必须核对三项身份。 */
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoAdapterRegistrationHandle
{
    /** 游戏实例注册表作用域，不是世界RunToken。 */
    FGuid InstanceId;
    /** 每次注册唯一GUID，注销后不会复用。 */
    FGuid RegistrationId;
    /** 正注册代次。 */
    uint64 Generation = 0;
    /** 无效句柄表示注册未被接受。 */
    bool IsValid() const { return InstanceId.IsValid() && RegistrationId.IsValid() && Generation > 0; }
    /** 完整比较支持拒绝跨实例或旧代次注销。 */
    bool operator==(const FDivineBeastsDemoAdapterRegistrationHandle& Other) const
    { return InstanceId == Other.InstanceId && RegistrationId == Other.RegistrationId && Generation == Other.Generation; }
};

/** 准备输入：自包含合同，不允许保存调用栈引用或把开发夹具当玩家事实。 */
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoScenarioContext
{
    /** 本次运行身份；适配完成必须原样携回。 */
    FDivineBeastsDemoRunToken Token;
    /** 只允许本世界有效对象；退出后不得解引用旧Actor。 */
    TWeakObjectPtr<UWorld> World;
    /** 已预检案例值快照，作者刷新不能改变活动运行合同。 */
    FDivineBeastsDemoScenarioDefinition Scenario;
    /** 单调时钟截止秒数，来自案例配置，不是UTC或性能预算。 */
    double PrepareDeadlineSeconds = 0.0;
    /** UI样板控件弱引用；只允许由Monolith制作且已真实加载的指定对象。 */
    TWeakObjectPtr<UObject> SampleWidget;
    /** 首次快照及后续服务事件的只读脱敏说明，最多4096字符；不改变命令资格/受理/终态。
     * 适配按Token原样回传，World检查世界/代次/活动状态后通知UI；不得用轮询驱动。
     */
    TFunction<void(const FDivineBeastsDemoRunToken&, const FString&)> PublishObservation;
};

/** 每次操作最多完成一次，游戏线程回调；失败/取消保持真实结果，不自动签收。 */
using FDivineBeastsDemoOperationCompletion = TFunction<void(const FDivineBeastsDemoRunToken&, const FGamePlatformResult&)>;

/** 供UI与证据读取的世界运行快照，不包含可变执行器或服务器权威。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoRunSnapshot
{
    GENERATED_BODY()
    /** 当前/最近运行身份，无运行时无效。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FDivineBeastsDemoRunToken Token;
    /** 独立案例身份，禁止地图路径作为已编目身份。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName ScenarioId;
    /** 本次运行主功能身份。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName FeatureId;
    /** 生命周期真实阶段。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") EDivineBeastsDemoRunState State = EDivineBeastsDemoRunState::Idle;
    /** 同一世界是否有尚未完成的用户操作，用于防止重入写入。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") bool bOperationPending = false;
    /** 最近准备/操作的原始结果，清理和人工结论不覆盖。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FGamePlatformResult OperationResult;
    /** 停止/取消清理的真实结果，未清理为NotExecuted。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FGamePlatformResult CleanupResult;
    /** 清理尚未成功确认时继续拥有本次实例/资源；失败可显式重试，不能覆盖新运行。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") bool bCleanupPending = false;
    /** 本Run清理尝试序号；用于诊断，不代表清理完成。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") int32 CleanupAttempt = 0;
    /** 清理完成后的生命周期终态，清理失败时Failed；仍保留原始OperationResult。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FGamePlatformResult TerminalResult;
    /** 真正完成的用户操作数量；只完成Prepare或立即Stop不能冒充功能已执行。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") int64 CompletedOperations = 0;
    /** 本运行最后一次只读服务事件序号；不是操作完成数。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") int32 ObservationRevision = 0;
    /** 最后服务观察，不代表当前授权或最终结果；新运行清空。禁止包含凭据/响应原文。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FString ObservationText;
};

/** 待审运行证据输入；只含版本/原始结果/合同，不含审核人或人工通过字段。 */
struct DIVINEBEASTSDEMOCLIENT_API FDivineBeastsDemoRunEvidence
{
    FDivineBeastsDemoRunSnapshot Run;
    FDivineBeastsDemoScenarioDefinition Scenario;
    TArray<FDivineBeastsDemoReviewCaseDefinition> ReviewCases;
    /** 本次用例各自功能目的的冻结副本；不得用刷新后的目的解释旧运行。 */
    TArray<FDivineBeastsDemoFeatureEntry> Features;
    /** 开始运行时冻结的作者源SHA256，刷新不覆盖历史版本。 */
    FString SourceFingerprint;
    /** 原始状态事件只含状态/错误码，动态诊断文本不进入日志。 */
    TArray<FString> EventRecords;
};

/** 世界状态变化事件；参数为只读值快照，订阅者失活时解绑。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FDivineBeastsDemoRunChanged, const FDivineBeastsDemoRunSnapshot&);
