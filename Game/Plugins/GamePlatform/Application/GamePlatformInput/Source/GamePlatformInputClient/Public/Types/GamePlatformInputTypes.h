#pragma once
#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "InputTriggers.h"
#include "InputCoreTypes.h"
#include "Types/GamePlatformResult.h"
#include "UObject/PrimaryAssetId.h"
#include "GamePlatformInputTypes.generated.h"

/** 仅本地请求语义，不能作为服务器权威动作协议；名称由唯一注册函数映射为平台输入标签。 */
UENUM(BlueprintType)
enum class EGamePlatformInputSemantic : uint8
{ Move, LookDelta, LookRate, AttackPrimary, AbilitySlot1, AbilitySlot2, AbilitySlot3, AbilitySlot4, Interact, TargetLock, Menu, Confirm, Cancel };
/** 位掩码分别抑制；文本场景由组合者同时申请三个Gameplay位，UI关闭命令保留。 */
enum class EGamePlatformInputChannel : uint8 { Move = 1, Look = 2, Actions = 4, UICommands = 8, TextEntry = 16 };
/** Delta不再乘帧间隔；Rate必须由最终视角消费者乘且只乘一次帧间隔。 */
UENUM(BlueprintType)
enum class EGamePlatformInputUnit : uint8 { Boolean, NormalizedAxis, DegreesDelta, DegreesPerSecond };
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
struct FGamePlatformInputTouchHandle : FGamePlatformInputIdentity {};

/** 值对象不含原始按键、文字、票据；仅游戏线程短期消费，不构成可回放个人键盘日志。 */
struct FGamePlatformInputEvent
{
    EGamePlatformInputSemantic Semantic = EGamePlatformInputSemantic::Move;
    ETriggerEvent Phase = ETriggerEvent::None;
    FInputActionValue Value;
    EGamePlatformInputUnit Unit = EGamePlatformInputUnit::NormalizedAxis;
    EGamePlatformInputEndReason EndReason = EGamePlatformInputEndReason::None;
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
    bool bPreferencesSaved = false;
    uint64 ProfileGeneration = 0;
    uint64 BindingGeneration = 0;
    uint64 SettingsRevision = 0;
    uint8 BlockedChannels = 0;
    int32 ContextLeaseCount = 0;
    int32 OwnedBindingCount = 0;
    int32 TouchSourceCount = 0;
    FGamePlatformResult Result;
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
