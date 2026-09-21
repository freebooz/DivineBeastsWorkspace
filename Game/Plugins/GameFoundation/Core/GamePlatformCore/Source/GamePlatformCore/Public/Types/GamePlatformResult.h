#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/NoExportTypes.h"
#include "GamePlatformResult.generated.h"

/** 同步或异步调用的中立结果分类；描述结果，不触发取消、回滚、重试或资源释放。 */
UENUM(BlueprintType)
enum class EGamePlatformResultStatus : uint8
{
    /** 尚未执行；默认值必须不能被误认为成功。 */
    NotExecuted,
    /** 调用方确认操作成功完成。 */
    Succeeded,
    /** 操作失败，Code给出可检索原因。 */
    Failed,
    /** 操作被取消；不表示业务补偿已完成。 */
    Cancelled,
    /** 当前实现或环境不支持所请求能力。 */
    Unsupported
};

/**
 * 自包含的结果值；Code用于程序分支，Message供诊断而非本地化UI或敏感数据传输。
 * 工厂保证失败／不支持诊断完整；公开C++字段为调用方契约，直接赋值需自行保持一致。
 * 无UObject或回调所有权；可在线程间复制，禁止无同步并发修改同一实例。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCORE_API FGamePlatformResult
{
    GENERATED_BODY()

    /** 默认未执行；仅显式Succeeded可能成功。蓝图只能读取。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Core")
    EGamePlatformResultStatus Status = EGamePlatformResultStatus::NotExecuted;

    /** 机器可读诊断；成功时为NAME_None，失败／不支持缺码由工厂补充明确错误码。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Core")
    FName Code;

    /** 人类可读中文诊断或调用者提供的说明；调用者负责脱敏。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Core")
    FString Message;

    /** 显式成功结果；Code与Message均为空，不表示任何尚未执行的工作已经完成。 */
    static FGamePlatformResult Success();

    /** 失败结果；Code为None时使用MissingFailureCode并保留原Message，空说明自动补充诊断。 */
    static FGamePlatformResult Failure(FName Code, FString Message);

    /** 取消结果，Code固定Cancelled；空Message自动补充取消说明，不执行实际取消动作。 */
    static FGamePlatformResult Cancelled(FString Message);

    /** 不支持结果；Code为None时使用MissingUnsupportedCode，始终保持非成功状态。 */
    static FGamePlatformResult Unsupported(FName Code, FString Message);

    /** 仅Status为Succeeded且Code为None返回true；默认值、矛盾的成功错误码均非成功。 */
    bool IsSuccess() const;
};
