#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformSettingTypes.generated.h"

/** 设置值安全上限；用于持久化、环境变量和命令行边界，避免无界字符串进入设置快照。 */
namespace GamePlatformSettingsLimits
{
    inline constexpr int32 MaxStringLength = 4096;
}

/** 统一设置值类型；禁止把所有设置退化成无约束字符串。 */
UENUM(BlueprintType)
enum class EGamePlatformSettingValueType : uint8
{
    Boolean,
    Integer,
    Number,
    String,
    Name
};

/** 设置持久化/覆盖作用域。 */
UENUM(BlueprintType)
enum class EGamePlatformSettingScope : uint8
{
    Project,
    User,
    Session,
    Server
};

/** 设置允许运行的端侧。 */
UENUM(BlueprintType)
enum class EGamePlatformSettingRuntimeScope : uint8
{
    Any,
    Client,
    Server
};

/** 设置应用时机；RestartRequired 仅表示调用方需在重启后消费，不由底层强制重启进程。 */
UENUM(BlueprintType)
enum class EGamePlatformSettingApplyMode : uint8
{
    Immediate,
    Deferred,
    RestartRequired
};

/** 配置覆盖层；枚举顺序不是优先级，解析器按客户端/服务器显式顺序计算。 */
UENUM(BlueprintType)
enum class EGamePlatformSettingLayer : uint8
{
    PlatformDefault,
    ProjectDefault,
    ProviderDefault,
    User,
    Session,
    ServerDefault,
    Deployment,
    Environment,
    CommandLine,
    MAX UMETA(Hidden)
};

/** 变化原因，用于批量事件与诊断。 */
UENUM(BlueprintType)
enum class EGamePlatformSettingsChangeReason : uint8
{
    Apply,
    Reset,
    Reload,
    Migration,
    Persistence,
    ProviderChanged
};

/**
 * 类型安全设置值。
 * 只读取与 Type 对应的字段；序列化保留类型，避免字符串解析污染运行热路径。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSETTINGSRUNTIME_API FGamePlatformSettingValue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    EGamePlatformSettingValueType Type = EGamePlatformSettingValueType::Boolean;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    bool BoolValue = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    int64 IntegerValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    double NumberValue = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    FString StringValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    FName NameValue = NAME_None;

    static FGamePlatformSettingValue MakeBool(bool Value);
    static FGamePlatformSettingValue MakeInteger(int64 Value);
    static FGamePlatformSettingValue MakeNumber(double Value);
    static FGamePlatformSettingValue MakeString(FString Value);
    static FGamePlatformSettingValue MakeName(FName Value);

    /** 严格按目标类型解析文本；用于 INI/环境变量/命令行边界，不用于热路径。 */
    static bool TryParse(
        EGamePlatformSettingValueType InType,
        const FString& Text,
        FGamePlatformSettingValue& OutValue);

    /** 判断当前值与声明类型一致且数值为有限值。 */
    bool IsValidForType(EGamePlatformSettingValueType ExpectedType) const;

    /** 语义比较；Number 使用稳定小容差。 */
    bool Equals(const FGamePlatformSettingValue& Other) const;

    /** 诊断文本；敏感设置始终返回 <redacted>。 */
    FString ToDiagnosticString(bool bSensitive) const;
};

/**
 * 一个设置项的稳定描述。
 * Descriptor 属于 Provider 的公开契约，不包含项目对象指针或资产硬引用。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSETTINGSRUNTIME_API FGamePlatformSettingDescriptor
{
    GENERATED_BODY()

    /** 全插件唯一稳定标识，例如 Platform.Display.VSync。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    FName SettingId = NAME_None;

    /** 用于批量 Reset/展示分组的稳定分类。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    FName Category = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    EGamePlatformSettingValueType ValueType = EGamePlatformSettingValueType::Boolean;

    /** Provider 提供的默认值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    FGamePlatformSettingValue DefaultValue;

    /** DefaultValue 写入的默认层；只允许三种默认层或 ServerDefault。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    EGamePlatformSettingLayer DefaultLayer = EGamePlatformSettingLayer::ProviderDefault;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    EGamePlatformSettingScope PersistenceScope = EGamePlatformSettingScope::User;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    EGamePlatformSettingRuntimeScope RuntimeScope = EGamePlatformSettingRuntimeScope::Any;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    EGamePlatformSettingApplyMode ApplyMode = EGamePlatformSettingApplyMode::Deferred;

    /** 仅数值类型使用；开启后必须与 ValueType 类型一致。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Validation")
    bool bHasMinimum = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Validation")
    FGamePlatformSettingValue MinimumValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Validation")
    bool bHasMaximum = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Validation")
    FGamePlatformSettingValue MaximumValue;

    /** 空数组表示所有平台；值使用 IniPlatformName，例如 Windows/Android。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Platform")
    TArray<FName> PlatformAllowList;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    bool bRestartRequired = false;

    /** 敏感值不得进入普通日志、Dump 或遥测。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    bool bSensitive = false;

    /** 开发专用设置在 Shipping 中不应被注册。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    bool bDevelopmentOnly = false;

    /** 描述本设置最后一次结构版本；用于诊断和迁移规则选择。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
    int32 SchemaVersion = 1;
};

/** 最终解析后的单个设置值。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSETTINGSRUNTIME_API FGamePlatformResolvedSetting
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    FName SettingId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    FGamePlatformSettingValue Value;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    EGamePlatformSettingLayer SourceLayer = EGamePlatformSettingLayer::ProviderDefault;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    int64 Revision = 0;
};

/** 单项变化；Old/New 均为值拷贝，不引用底层注册表。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSETTINGSRUNTIME_API FGamePlatformSettingChange
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    FName SettingId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    FName Category = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    FGamePlatformSettingValue OldValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    FGamePlatformSettingValue NewValue;
};

/** 批量变化集合；一次 Apply/Reload/Migration 只广播一批，避免事件风暴。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSETTINGSRUNTIME_API FGamePlatformSettingsChangeSet
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    EGamePlatformSettingsChangeReason Reason = EGamePlatformSettingsChangeReason::Apply;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    TArray<FGamePlatformSettingChange> Changes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    int64 Revision = 0;
};

/** 只读解析快照；Gameplay/UI 查询使用 TMap 接近 O(1)，不反复解析 Profile/INI。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSETTINGSRUNTIME_API FGamePlatformSettingsSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    TMap<FName, FGamePlatformResolvedSetting> Values;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    int64 Revision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    int32 SchemaVersion = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    bool bDirty = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    bool bSaveInFlight = false;

    /** 最近一次系统级结果；敏感值不进入 Message。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings")
    FGamePlatformResult LastResult;
};

/** 运行诊断只保留计数，不记录敏感值或玩家输入内容。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSETTINGSRUNTIME_API FGamePlatformSettingsRuntimeDiagnostics
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int32 ProviderCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int32 DescriptorCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int32 SubscriptionCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 ResolveCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 ApplyCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 SaveRequestCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 MigrationCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 RejectedMutationCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Settings|Diagnostics")
    int64 SubscriberCallbackCount = 0;
};

/** Runtime 订阅句柄；完整作用域与代次阻止跨 GameInstance/旧实例误撤销。 */
struct FGamePlatformSettingsRuntimeSubscription
{
    FGuid ScopeId;
    FGuid Id;
    uint64 Generation = 0;

    bool IsValid() const
    {
        return ScopeId.IsValid() && Id.IsValid() && Generation != 0;
    }
};
