#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

/** 调试快照来源视角。客户端与服务器视角不得混淆。 */
enum class EGamePlatformDebugSourceView : uint8
{
    Client,
    Server,
    LocalAuthority
};

/** 调试字段严重度，仅用于诊断展示。 */
enum class EGamePlatformDebugSeverity : uint8
{
    Info,
    Warning,
    Error
};

/** 调试字段的中立值类型。 */
enum class EGamePlatformDebugValueType : uint8
{
    String,
    Integer,
    Number,
    Boolean,
    Vector,
    Name
};

/** Provider（状态提供者）采集成本。 */
enum class EGamePlatformDebugProviderCost : uint8
{
    Cheap,
    Moderate,
    Expensive
};

/** 调试命令是否可能改变开发期状态。 */
enum class EGamePlatformDebugCommandKind : uint8
{
    ReadOnly,
    Mutating
};

/** 单个安全调试字段；Sensitive（敏感）字段在输出前必须被过滤。 */
struct GAMEPLATFORMDEBUG_API FGamePlatformDebugField
{
    FName Key = NAME_None;
    FString DisplayName;
    EGamePlatformDebugValueType ValueType = EGamePlatformDebugValueType::String;
    FString Value;
    EGamePlatformDebugSeverity Severity = EGamePlatformDebugSeverity::Info;
    bool bSensitive = false;
};

/** 中立调试快照。禁止在此结构中保存 Actor/UObject 的完整序列化。 */
struct GAMEPLATFORMDEBUG_API FGamePlatformDebugSnapshot
{
    FGuid RequestId;
    FName CategoryId = NAME_None;
    FGuid TargetId;
    int32 TargetGeneration = 0;
    int32 WorldGeneration = 0;
    EGamePlatformDebugSourceView SourceView = EGamePlatformDebugSourceView::Client;
    uint64 Revision = 0;
    double TimestampSeconds = 0.0;
    TArray<FGamePlatformDebugField> Fields;
    bool bTruncated = false;
    FString FailureReason;

    void AddField(
        FName Key,
        FString DisplayName,
        FString Value,
        EGamePlatformDebugValueType ValueType = EGamePlatformDebugValueType::String,
        EGamePlatformDebugSeverity Severity = EGamePlatformDebugSeverity::Info,
        bool bSensitive = false)
    {
        FGamePlatformDebugField& Field = Fields.AddDefaulted_GetRef();
        Field.Key = Key;
        Field.DisplayName = MoveTemp(DisplayName);
        Field.Value = MoveTemp(Value);
        Field.ValueType = ValueType;
        Field.Severity = Severity;
        Field.bSensitive = bSensitive;
    }
};

/** 对调试对象的弱引用身份。World Travel（世界切换）后旧目标自然失效。 */
struct GAMEPLATFORMDEBUG_API FGamePlatformDebugTarget
{
    FGuid DebugTargetId;
    TWeakObjectPtr<AActor> Actor;
    TWeakObjectPtr<UWorld> World;
    int32 TargetGeneration = 0;
};

/** Provider（状态提供者）的采集上下文。 */
struct GAMEPLATFORMDEBUG_API FGamePlatformDebugCollectContext
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AActor> Target;
    EGamePlatformDebugSourceView SourceView = EGamePlatformDebugSourceView::Client;
    FGuid RequestId;
    FString Filter;
    bool bAllowExpensive = false;
};

/** Console Manager（控制台管理器）命令元数据。 */
struct GAMEPLATFORMDEBUG_API FGamePlatformDebugCommandDescriptor
{
    FName Name = NAME_None;
    FString Help;
    FName Category = NAME_None;
    FString Args;
    EGamePlatformDebugCommandKind Kind = EGamePlatformDebugCommandKind::ReadOnly;
    FName RequiredPrivilege = TEXT("Developer");
    TArray<FString> AllowedBuilds;
};

/** 第一版固定安全上限，防止调试本身制造显著负载。 */
namespace GamePlatformDebugLimits
{
    static constexpr int32 MaxFieldsPerSnapshot = 128;
    static constexpr int32 MaxStringCharacters = 512;
    static constexpr int32 MaxPayloadCharacters = 32 * 1024;
    static constexpr int32 MaxCommandArgumentCharacters = 256;
}
