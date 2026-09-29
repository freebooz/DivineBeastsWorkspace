#pragma once

// GamePlatformSave 公共值类型。
// 这里只公开跨游戏稳定契约；磁盘路径、文件句柄和具体存储实现全部保持在Private目录。
#include "CoreMinimal.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformSaveTypes.generated.h"

/**
 * 本地存档逻辑键。
 *
 * Namespace：由领域调用方声明的稳定命名空间，例如 LocalTutorial。
 * ProfileKey：调用方提供的脱敏稳定本地档案键；不得传入密码、Token或服务器凭据。
 * SlotName：命名空间内部槽位名。
 *
 * 三个字段只参与稳定摘要计算，不直接拼接到磁盘路径。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSAVECLIENT_API FGamePlatformSaveKey
{
    GENERATED_BODY()

    /** 跨游戏稳定命名空间；不能为空。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save")
    FName Namespace;

    /** 脱敏稳定档案键；用于本地多账号隔离，不是在线身份真源。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save")
    FString ProfileKey;

    /** 命名空间内部槽位名；不能为空。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save")
    FString SlotName;
};

/**
 * 非权威本地存档记录。
 *
 * Payload内容由Namespace所有者定义；GamePlatformSave只负责版本、完整性和持久化。
 * SchemaVersion必须为正整数，升级迁移由对应IGamePlatformSaveProvider负责。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSAVECLIENT_API FGamePlatformSaveRecord
{
    GENERATED_BODY()

    /** 记录逻辑键。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save")
    FGamePlatformSaveKey Key;

    /** Namespace业务模式版本；与插件内部Envelope格式版本相互独立。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save")
    int32 SchemaVersion = 1;

    /** 调用方序列化后的二进制载荷；禁止承载服务器权威或敏感秘密。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save")
    TArray<uint8> Payload;
};

/** 异步本地存档操作类型。 */
UENUM(BlueprintType)
enum class EGamePlatformSaveOperation : uint8
{
    Load,
    Save,
    Delete
};

/**
 * 异步操作完成结果。
 * 回调始终在游戏线程执行；Subsystem销毁时尚未完成的请求返回Cancelled。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSAVECLIENT_API FGamePlatformSaveOperationResult
{
    GENERATED_BODY()

    /** 完成的操作类型。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
    EGamePlatformSaveOperation Operation = EGamePlatformSaveOperation::Load;

    /** 结构化执行结果。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
    FGamePlatformResult Result;

    /** Load成功时返回记录；Save成功时回显已写入记录；Delete时为空记录。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
    FGamePlatformSaveRecord Record;

    /** 主档缺失或损坏后是否成功使用.bak备份。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
    bool bRecoveredFromBackup = false;

    /** 是否由Provider把旧SchemaVersion迁移到当前版本。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
    bool bMigrated = false;
};

/** 低频运行诊断；只包含计数，不记录ProfileKey、Payload或其他敏感内容。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSAVECLIENT_API FGamePlatformSaveDiagnostics
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save|Diagnostics")
    int32 ProviderCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save|Diagnostics")
    int32 PendingRequestCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save|Diagnostics")
    int64 StartedRequestCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save|Diagnostics")
    int64 CompletedRequestCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save|Diagnostics")
    int64 FailedRequestCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save|Diagnostics")
    int64 RecoveredBackupCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save|Diagnostics")
    int64 MigratedRecordCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save|Diagnostics")
    int64 RejectedRequestCount = 0;
};

/**
 * 异步请求句柄。
 * ScopeId与Generation阻止旧GameInstance请求误操作新实例；当前版本不提供强制中断磁盘系统调用。
 */
struct GAMEPLATFORMSAVECLIENT_API FGamePlatformSaveRequestHandle
{
    FGuid ScopeId;
    FGuid Id;
    uint64 Generation = 0;

    bool IsValid() const
    {
        return ScopeId.IsValid() && Id.IsValid() && Generation != 0;
    }
};
