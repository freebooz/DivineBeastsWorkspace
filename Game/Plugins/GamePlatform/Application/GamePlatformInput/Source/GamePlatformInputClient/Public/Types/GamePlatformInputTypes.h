// 平台本地玩家输入纯值/身份契约；所有者与配置代次分离，UI读取快照，客户端输入不形成服务器授权；保存提交与落盘确认分开。
#pragma once
#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "InputTriggers.h"
#include "InputCoreTypes.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformResult.h"
#include "UObject/PrimaryAssetId.h"
#include "GamePlatformInputTypes.generated.h"

/**
 * 旧固定输入语义兼容枚举。
 * 新项目代码应使用FGamePlatformInputSemanticId/Descriptor；本枚举保留既有资产/API兼容，不能继续追加项目技能语义。
 */
UENUM(BlueprintType)
enum class EGamePlatformInputSemantic : uint8
{ Move, LookDelta, LookRate, AttackPrimary, AbilitySlot1, AbilitySlot2, AbilitySlot3, AbilitySlot4, Interact, TargetLock, Menu, Confirm, Cancel };

/**
 * 平台长期稳定的内建输入语义。
 * 只包含跨游戏公共导航/视角/UI/交互语义；攻击、技能槽和目标锁定不得再进入该枚举。
 */
UENUM(BlueprintType)
enum class EGamePlatformBuiltInInputSemantic : uint8
{
    Move,
    LookDelta,
    LookRate,
    Interact,
    Menu,
    Confirm,
    Cancel
};
/** 位掩码分别抑制；文本场景由组合者同时申请三个Gameplay位，UI关闭命令保留。 */
enum class EGamePlatformInputChannel : uint8 { Move = 1, Look = 2, Actions = 4, UICommands = 8, TextEntry = 16 };
/** Delta不再乘帧间隔；Rate必须由最终视角消费者乘且只乘一次帧间隔。 */
UENUM(BlueprintType)
enum class EGamePlatformInputUnit : uint8 { Boolean, NormalizedAxis, DegreesDelta, DegreesPerSecond };
/** 运行前一次编译的值处理策略；项目自定义布尔/轴动作通常使用Passthrough。 */
UENUM(BlueprintType)
enum class EGamePlatformInputValuePolicy : uint8 { Passthrough, MoveAxis, LookDelta, LookRate };

/**
 * 稳定可扩展输入语义标识。
 * 平台、MOBA或项目层均可声明自己的GameplayTag；高频运行时不会直接以Tag做路由查找。
 */
USTRUCT(BlueprintType)
struct FGamePlatformInputSemanticId
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Semantic")
    FGameplayTag Tag;
    bool IsValid() const { return Tag.IsValid(); }
    bool operator==(const FGamePlatformInputSemanticId& Other) const { return Tag == Other.Tag; }
};

/**
 * 输入语义的数据化描述。
 * Profile准备阶段会把本结构编译成CompactSlot（紧凑槽位），之后鼠标/摇杆高频路径只使用数组索引。
 */
USTRUCT(BlueprintType)
struct FGamePlatformInputSemanticDescriptor
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Semantic")
    FGamePlatformInputSemanticId SemanticId;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Semantic")
    EGamePlatformInputUnit Unit = EGamePlatformInputUnit::Boolean;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Semantic")
    EInputActionValueType ValueType = EInputActionValueType::Boolean;
    /** 必须是一个已知单通道位；阻断仍保持uint8位运算O(1)。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Semantic")
    uint8 ChannelMask = static_cast<uint8>(EGamePlatformInputChannel::Actions);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Semantic")
    EGamePlatformInputValuePolicy ValuePolicy = EGamePlatformInputValuePolicy::Passthrough;
};
/** 当前本地玩家最近一次由平台确认的输入设备族；只用于本地表现/提示，不参与服务器权威。 */
UENUM(BlueprintType)
enum class EGamePlatformInputDeviceFamily : uint8 { Unknown, KeyboardMouse, Gamepad, Touch };

/**
 * 本地无障碍/舒适度偏好。
 * 这些值只影响客户端输入解释，不修改服务器权威规则，也不在高频输入路径做磁盘IO。
 */
USTRUCT(BlueprintType)
struct FGamePlatformInputAccessibilitySettings
{
    GENERATED_BODY()

    /** 统一视角灵敏度倍率；0.1..5.0。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Accessibility")
    double LookSensitivityMultiplier = 1.0;

    /** 是否反转水平视角。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Accessibility")
    bool bInvertLookX = false;

    /** 是否反转垂直视角。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Accessibility")
    bool bInvertLookY = false;

    /** 移动轴附加死区倍率；1.0表示使用Profile默认值，范围0.5..2.0。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Accessibility")
    double MoveDeadZoneMultiplier = 1.0;

    /** 移动端Touch视角额外灵敏度倍率；与通用LookSensitivity相乘，范围0.25..3.0。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Accessibility")
    double TouchLookSensitivityMultiplier = 1.0;

    /** 移动端虚拟移动摇杆幅度倍率；用于小屏/大屏手感补偿，范围0.5..1.5。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Accessibility")
    double TouchMoveScale = 1.0;
};
/** 中断不是释放施法；原生Completed也不代表服务器授权或技能成功。 */
enum class EGamePlatformInputEndReason : uint8 { None, NativeCompleted, NativeCanceled, Blocked, ReceiverChanged, ContextRemoved, ProfileReleased, FocusLost };

/** 全成员参与验证；不同用途继承不同类型，旧LocalPlayer或配置代次不可操作新状态。 */
struct FGamePlatformInputIdentity
{
    FGuid ScopeId;
    FGuid Id;
    uint64 Generation = 0;
    bool IsValid() const { return ScopeId.IsValid() && Id.IsValid() && Generation != 0; }
    bool operator==(const FGamePlatformInputIdentity& Other) const { return ScopeId == Other.ScopeId && Id == Other.Id && Generation == Other.Generation; }
};
struct FGamePlatformInputProfileHandle : FGamePlatformInputIdentity {};
struct FGamePlatformInputContextHandle : FGamePlatformInputIdentity {};
struct FGamePlatformInputBlockHandle : FGamePlatformInputIdentity {};
struct FGamePlatformInputBindingHandle : FGamePlatformInputIdentity {};
struct FGamePlatformInputSubscription : FGamePlatformInputIdentity {};
/** 输入状态变化订阅句柄；与高频动作事件订阅分离，避免 UI/组合根把状态观察混入输入热路径。 */
struct FGamePlatformInputStateSubscription : FGamePlatformInputIdentity {};
struct FGamePlatformInputTouchHandle : FGamePlatformInputIdentity {};

/** 值对象不含原始按键、文字、票据；仅游戏线程短期消费，不构成可回放个人键盘日志。 */
struct FGamePlatformInputEvent
{
    EGamePlatformInputSemantic Semantic = EGamePlatformInputSemantic::Move;
    /** 新语义体系下的稳定标识；旧枚举事件兼容时也会填充对应Tag。 */
    FGamePlatformInputSemanticId SemanticId;
    /** true表示该事件来自旧EGamePlatformInputSemantic兼容入口。 */
    bool bHasLegacySemantic = false;
    ETriggerEvent Phase = ETriggerEvent::None;
    FInputActionValue Value;
    EGamePlatformInputUnit Unit = EGamePlatformInputUnit::NormalizedAxis;
    EGamePlatformInputEndReason EndReason = EGamePlatformInputEndReason::None;
    /** 事件产生时的最近设备族；硬件动作未知时使用服务当前设备族。 */
    EGamePlatformInputDeviceFamily DeviceFamily = EGamePlatformInputDeviceFamily::Unknown;
    uint64 BindingGeneration = 0;
    uint64 Sequence = 0;
};
/** 配置与当前绑定独立准备，不等待Loading最终Ready；输入是否开放还受调用者令牌影响。 */
struct FGamePlatformInputSnapshot
{
    bool bProfilePrepared = false;
    bool bBindingsReady = false;
    bool bMappingsApplied = false;
    bool bGameplayInputEnabled = false;
    /** 保留旧读取身份；只有可验证落盘结果才可为true，当前原生void保存路径始终false。 */
    bool bPreferencesSaved = false;
    /** 最近一次显式Save已提交给Enhanced Input和INI；实际落盘成功/失败未知，修改偏好后清零。 */
    bool bPreferencesSaveSubmitted = false;
    /** 最近一次由平台/触控桥确认的设备族，用于提示图标和设备特定UI。 */
    EGamePlatformInputDeviceFamily ActiveDeviceFamily = EGamePlatformInputDeviceFamily::Unknown;
    /** 当前无障碍/舒适度偏好快照。 */
    FGamePlatformInputAccessibilitySettings Accessibility;
    uint64 ProfileGeneration = 0;
    uint64 BindingGeneration = 0;
    uint64 SettingsRevision = 0;
    /** 当前设备族状态修订号；仅在设备族真实变化时递增，供UI避免重复刷新提示。 */
    uint64 DeviceRevision = 0;
    uint8 BlockedChannels = 0;
    int32 ContextLeaseCount = 0;
    int32 OwnedBindingCount = 0;
    int32 TouchSourceCount = 0;
    FGamePlatformResult Result;
};
/**
 * LocalPlayer（本地玩家）作用域轻量诊断。
 * 只包含计数与耗时，不记录原始按键、文本、触摸坐标或玩家隐私数据；供Debug/Telemetry上层按需读取。
 */
struct FGamePlatformInputDiagnostics
{
    FGuid ScopeId;
    bool bMaintenanceTickerScheduled = false;
    EGamePlatformInputDeviceFamily ActiveDeviceFamily = EGamePlatformInputDeviceFamily::Unknown;
    /** 当前设备族修订号，与Snapshot保持同一语义。 */
    uint64 DeviceRevision = 0;
    int32 ContextLeaseCount = 0;
    int32 BlockLeaseCount = 0;
    int32 BindingCount = 0;
    int32 SubscriptionCount = 0;
    int32 TouchSourceCount = 0;
    int64 TotalInputEventsPublished = 0;
    /** 本实例累计执行的订阅者回调次数；与事件数分开，避免多订阅者把单事件重复计数。 */
    int64 TotalSubscriberCallbacks = 0;
    int64 TotalDeviceFamilyChanges = 0;
    int64 TotalMappingRebuildRequests = 0;
    int64 TotalMaintenanceTicks = 0;
    int64 TotalExpiredOwnersCollected = 0;
    double LastMaintenanceMilliseconds = 0.0;
    double MaxMaintenanceMilliseconds = 0.0;
};

/** 稳定行+槽+设备约束，非数组下标；第一版仅键盘/鼠标数字键与手柄数字键重绑。 */
struct FGamePlatformInputMapping
{
    FName RowName;
    int32 Slot = 0;
    FKey Key;
    FKey DefaultKey;
    bool bGamepad = false;
};
/** 预检只读；有冲突不做隐式覆盖。调用Apply必须重新校验，不能持有旧预检当授权。 */
struct FGamePlatformInputRebindPreview
{
    FGamePlatformResult Result;
    TArray<FName> ConflictingRows;
};
