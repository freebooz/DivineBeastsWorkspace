#include "Subsystems/GamePlatformInputLocalPlayerSubsystem.h"

#include "Definitions/GamePlatformInputProfileDefinition.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Policy/InputPolicy.h"
#include "Services/GamePlatformInputServices.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/SecureHash.h"
#include "UObject/StrongObjectPtr.h"
#include <array>

namespace Policy = GamePlatformInputPolicy;

namespace
{
/** 弱所有权租约维护频率4Hz；输入事件本身完全事件驱动，不通过Ticker轮询按键。 */
constexpr float InputMaintenanceIntervalSeconds = 0.25f;
/** 订阅者采用固定容量，防止错误UI或热重载代码造成无界增长。 */
constexpr int32 MaxInputSubscriptions = 32;
/** 当前控制器/Pawn切换只需极少绑定；硬上限防止重复热重载累计事件绑定。 */
constexpr int32 MaxInputBindings = 8;
/** Touch（触控）指针使用UE常见0..9范围，避免任意整数污染本地状态。 */
constexpr int32 MinTouchPointerId = 0;
constexpr int32 MaxTouchPointerId = 9;
/** 平台语义当前从Move连续排列到Cancel；固定槽数组避免高频事件路径使用哈希表。 */
constexpr int32 InputSemanticCount = static_cast<int32>(EGamePlatformInputSemantic::Cancel) + 1;
static_assert(static_cast<int32>(EGamePlatformInputSemantic::Move) == 0, "输入语义固定数组要求Move从0开始。");

/**
 * 返回当前目标平台的默认设备族。
 * Android/iOS默认Touch；桌面即使硬件支持触摸也默认键鼠，避免触屏PC启动时错误显示移动端按键提示。
 */
constexpr EGamePlatformInputDeviceFamily DefaultDeviceFamilyForTarget()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    return EGamePlatformInputDeviceFamily::Touch;
#else
    return EGamePlatformInputDeviceFamily::KeyboardMouse;
#endif
}

/** Profile禁用当前设备时选择合理回退；移动端优先Touch，桌面优先键鼠。 */
EGamePlatformInputDeviceFamily SelectFallbackDeviceFamily(const UGamePlatformInputProfileDefinition& Profile)
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    if (Profile.bEnableTouch) { return EGamePlatformInputDeviceFamily::Touch; }
    if (Profile.bEnableGamepad) { return EGamePlatformInputDeviceFamily::Gamepad; }
    if (Profile.bEnableKeyboardMouse) { return EGamePlatformInputDeviceFamily::KeyboardMouse; }
#else
    if (Profile.bEnableKeyboardMouse) { return EGamePlatformInputDeviceFamily::KeyboardMouse; }
    if (Profile.bEnableGamepad) { return EGamePlatformInputDeviceFamily::Gamepad; }
    if (Profile.bEnableTouch) { return EGamePlatformInputDeviceFamily::Touch; }
#endif
    return EGamePlatformInputDeviceFamily::Unknown;
}

/** 返回平台已知的全部输入阻断位。 */
constexpr uint8 AllInputChannels =
    static_cast<uint8>(EGamePlatformInputChannel::Move) |
    static_cast<uint8>(EGamePlatformInputChannel::Look) |
    static_cast<uint8>(EGamePlatformInputChannel::Actions) |
    static_cast<uint8>(EGamePlatformInputChannel::UICommands) |
    static_cast<uint8>(EGamePlatformInputChannel::TextEntry);

/** 将语义映射到Enhanced Input（增强输入）的期望值类型。 */
EInputActionValueType ExpectedValueType(EGamePlatformInputSemantic Semantic)
{
    switch (Semantic)
    {
    case EGamePlatformInputSemantic::Move:
    case EGamePlatformInputSemantic::LookDelta:
    case EGamePlatformInputSemantic::LookRate:
        return EInputActionValueType::Axis2D;
    default:
        return EInputActionValueType::Boolean;
    }
}

/** 输入重绑定只接受数字键、键盘、鼠标按钮或手柄按钮，不允许Touch/Gesture/连续轴作为离散重绑定键。 */
bool IsSupportedRebindKey(const FKey& Key)
{
    return Key.IsValid() && !Key.IsTouch() && !Key.IsGesture() &&
        !Key.IsAxis1D() && !Key.IsAxis2D() && !Key.IsAxis3D();
}

/** 本地设置键只允许安全ASCII，防止构造Config区段时引入控制字符或路径语义。 */
bool IsSafeSettingsKey(const FString& Key)
{
    if (Key.Len() < 8 || Key.Len() > 128) { return false; }
    for (const TCHAR Character : Key)
    {
        if (Character > 127 ||
            !(FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('-') || Character == TEXT('.')))
        {
            return false;
        }
    }
    return true;
}

/** 设备开关只属于本地Profile，不决定服务器权威。 */
bool IsDeviceEnabled(const UGamePlatformInputProfileDefinition& Profile, EGamePlatformInputDeviceFamily Family)
{
    switch (Family)
    {
    case EGamePlatformInputDeviceFamily::KeyboardMouse: return Profile.bEnableKeyboardMouse;
    case EGamePlatformInputDeviceFamily::Gamepad: return Profile.bEnableGamepad;
    case EGamePlatformInputDeviceFamily::Touch: return Profile.bEnableTouch;
    default: return false;
    }
}

/** 查找Profile中的上下文声明。 */
const FGamePlatformInputContextDefinition* FindContextDefinition(
    const UGamePlatformInputProfileDefinition& Profile,
    FName Name)
{
    return Profile.Contexts.FindByPredicate([Name](const FGamePlatformInputContextDefinition& Entry)
    {
        return Entry.Name == Name;
    });
}

/** 将FName转为策略层稳定UTF-8文本，仅用于进程内账本。 */
std::string ToPolicyName(FName Name)
{
    return TCHAR_TO_UTF8(*Name.ToString());
}

/** 将公开Identity转换为策略层64位Token；Generation在同一LocalPlayer作用域内单调不复用。 */
uint64 PolicyToken(const FGamePlatformInputIdentity& Identity)
{
    return Identity.Generation;
}

/** 返回当前映射行合法槽位。 */
bool IsValidMappingSlot(int32 Slot)
{
    return Slot >= static_cast<int32>(EPlayerMappableKeySlot::First) &&
        Slot <= static_cast<int32>(EPlayerMappableKeySlot::Seventh);
}

/** 构造给Enhanced Input注入的中性值。 */
FInputActionValue MakeNeutralValue(EInputActionValueType Type)
{
    return FInputActionValue(Type, FVector::ZeroVector);
}
}

struct FInputContextRecord
{
    FGamePlatformInputContextHandle Handle;
    FName ContextName;
    int32 Priority = 0;
    TWeakObjectPtr<UObject> Owner;
};

struct FInputBlockRecord
{
    FGamePlatformInputBlockHandle Handle;
    uint8 Channels = 0;
    FName Reason;
    TWeakObjectPtr<UObject> Owner;
};

struct FInputBindingRecord
{
    FGamePlatformInputBindingHandle Handle;
    TWeakObjectPtr<UEnhancedInputComponent> Component;
    TArray<uint32> NativeBindingHandles;
};

struct FInputSubscriptionRecord
{
    FGamePlatformInputSubscription Handle;
    TWeakObjectPtr<UObject> Owner;
    TFunction<void(const FGamePlatformInputEvent&)> Callback;
};

struct FInputTouchRecord
{
    FGamePlatformInputTouchHandle Handle;
    int32 PointerId = INDEX_NONE;
    EGamePlatformInputSemantic Semantic = EGamePlatformInputSemantic::Move;
    TWeakObjectPtr<UObject> Owner;
};

/** LocalPlayer（本地玩家）私有状态；Public API只暴露值快照和不可伪造句柄。 */
struct FGamePlatformInputScope
{
    FGuid ScopeId = FGuid::NewGuid();
    uint64 NextGeneration = 0;
    uint64 Sequence = 0;
    uint64 SettingsRevision = 0;
    uint64 BindingGeneration = 0;
    uint64 DeviceRevision = 0;

    FGamePlatformInputProfileHandle ProfileHandle;
    FGamePlatformDataLease ProfileLease;
    TWeakObjectPtr<UGamePlatformInputProfileDefinition> Profile;
    FString SettingsSection;

    TMap<EGamePlatformInputSemantic, TWeakObjectPtr<UInputAction>> Actions;
    TMap<FName, TWeakObjectPtr<UInputMappingContext>> Contexts;
    TSet<TWeakObjectPtr<UInputMappingContext>> PreferenceContextsOwned;

    TMap<FGuid, FInputContextRecord> ContextLeases;
    TMap<FGuid, FInputBlockRecord> Blocks;
    TMap<FGuid, FInputBindingRecord> Bindings;
    TMap<FGuid, FInputSubscriptionRecord> Subscriptions;
    TMap<FGuid, FInputTouchRecord> Touches;

    Policy::ContextLedger ContextLedger;
    Policy::BlockLedger BlockLedger;
    /** 固定语义槽避免高频Route/Interrupt路径的哈希查找和隐式节点分配。 */
    std::array<Policy::ActionGate, InputSemanticCount> ActionGates{};

    FGamePlatformInputAccessibilitySettings Accessibility;
    EGamePlatformInputDeviceFamily ActiveDeviceFamily = EGamePlatformInputDeviceFamily::Unknown;
    FGamePlatformResult LastResult;

    bool bProfilePrepared = false;
    bool bMappingsApplied = false;
    bool bPreferencesSaved = false;
    bool bHasFocus = true;
    bool bDispatching = false;
    bool bAdjustingContexts = false;
    int64 TotalInputEventsPublished = 0;
    int64 TotalSubscriberCallbacks = 0;
    int64 TotalDeviceFamilyChanges = 0;
    int64 TotalMappingRebuildRequests = 0;
    int64 TotalMaintenanceTicks = 0;
    int64 TotalExpiredOwnersCollected = 0;
    double LastMaintenanceMilliseconds = 0.0;
    double MaxMaintenanceMilliseconds = 0.0;
};

UGamePlatformInputLocalPlayerSubsystem::UGamePlatformInputLocalPlayerSubsystem() = default;
UGamePlatformInputLocalPlayerSubsystem::UGamePlatformInputLocalPlayerSubsystem(FVTableHelper& Helper) : Super(Helper) {}
UGamePlatformInputLocalPlayerSubsystem::~UGamePlatformInputLocalPlayerSubsystem() = default;

IGamePlatformInputService* IGamePlatformInputService::Get(ULocalPlayer& LocalPlayer)
{
    check(IsInGameThread());
    return LocalPlayer.GetSubsystem<UGamePlatformInputLocalPlayerSubsystem>();
}

bool UGamePlatformInputLocalPlayerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return !IsRunningDedicatedServer() && Cast<ULocalPlayer>(Outer) != nullptr;
}

void UGamePlatformInputLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Scope = MakeUnique<FGamePlatformInputScope>();

    // 目标平台决定初始设备族：Android/iOS默认Touch；桌面默认键鼠，触屏PC不会误用移动端提示。
    SetActiveDeviceFamily(DefaultDeviceFamilyForTarget(), false);

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Enhanced = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            Enhanced->ControlMappingsRebuiltDelegate.AddDynamic(this, &ThisClass::OnMappingsRebuilt);
            Enhanced->OnMappingContextAdded.AddDynamic(this, &ThisClass::OnContextAdded);
            Enhanced->OnMappingContextRemoved.AddDynamic(this, &ThisClass::OnContextRemoved);
        }
    }
}

void UGamePlatformInputLocalPlayerSubsystem::Deinitialize()
{
    check(IsInGameThread());

    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Enhanced = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            Enhanced->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &ThisClass::OnMappingsRebuilt);
            Enhanced->OnMappingContextAdded.RemoveDynamic(this, &ThisClass::OnContextAdded);
            Enhanced->OnMappingContextRemoved.RemoveDynamic(this, &ThisClass::OnContextRemoved);
        }
    }

    if (Scope)
    {
        // 按“动作中断→解绑→释放上下文→释放Data租约”顺序清理，避免旧按键穿透到下一个LocalPlayer生命周期。
        Interrupt(AllInputChannels, EGamePlatformInputEndReason::ProfileReleased);

        TArray<FGamePlatformInputBindingHandle> BindingHandles;
        for (const auto& Pair : Scope->Bindings) { BindingHandles.Add(Pair.Value.Handle); }
        for (const auto& Handle : BindingHandles) { UnbindInputReceiver(Handle); }

        TArray<FGamePlatformInputContextHandle> ContextHandles;
        for (const auto& Pair : Scope->ContextLeases) { ContextHandles.Add(Pair.Value.Handle); }
        for (const auto& Handle : ContextHandles) { ReleaseInputContext(Handle); }

        ReleasePreferences();

        if (Scope->ProfileLease.IsValid())
        {
            if (UGameInstance* Instance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr)
            {
                if (IGamePlatformDataService* Data = IGamePlatformDataService::Get(*Instance))
                {
                    Data->ReleaseDefinition(Scope->ProfileLease);
                }
            }
        }
        Scope.Reset();
    }

    Super::Deinitialize();
}

void UGamePlatformInputLocalPlayerSubsystem::PlayerControllerChanged(APlayerController* Controller)
{
    Super::PlayerControllerChanged(Controller);
    if (!Scope) { return; }

    // 控制器/Pawn切换时旧EnhancedInputComponent的绑定身份失效，必须先发送中断再精确移除本插件绑定。
    Interrupt(
        static_cast<uint8>(EGamePlatformInputChannel::Move) |
        static_cast<uint8>(EGamePlatformInputChannel::Look) |
        static_cast<uint8>(EGamePlatformInputChannel::Actions),
        EGamePlatformInputEndReason::ReceiverChanged);

    TArray<FGamePlatformInputBindingHandle> Handles;
    for (const auto& Pair : Scope->Bindings) { Handles.Add(Pair.Value.Handle); }
    for (const auto& Handle : Handles) { UnbindInputReceiver(Handle); }
}

bool UGamePlatformInputLocalPlayerSubsystem::CanMutate() const
{
    return Scope && !Scope->bDispatching;
}

bool UGamePlatformInputLocalPlayerSubsystem::IsCurrentOwner(TWeakObjectPtr<UObject> Owner) const
{
    if (!Scope || !Owner.IsValid()) { return false; }
    const ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer) { return false; }

    if (Owner.Get() == LocalPlayer || Owner->GetTypedOuter<ULocalPlayer>() == LocalPlayer)
    {
        return true;
    }

    const UWorld* World = Owner->GetWorld();
    return World && LocalPlayer->GetGameInstance() && World->GetGameInstance() == LocalPlayer->GetGameInstance();
}

void UGamePlatformInputLocalPlayerSubsystem::SetActiveDeviceFamily(
    EGamePlatformInputDeviceFamily DeviceFamily,
    bool bCountAsActivity)
{
    check(IsInGameThread());
    if (!Scope || Scope->ActiveDeviceFamily == DeviceFamily) { return; }

    Scope->ActiveDeviceFamily = DeviceFamily;
    ++Scope->DeviceRevision;
    if (bCountAsActivity) { ++Scope->TotalDeviceFamilyChanges; }
}

void UGamePlatformInputLocalPlayerSubsystem::ScheduleMaintenance()
{
    check(IsInGameThread());
    if (!Scope || IsRunningCommandlet() || TickerHandle.IsValid()) { return; }

    const bool bNeedsMaintenance =
        !Scope->ContextLeases.IsEmpty() ||
        !Scope->Blocks.IsEmpty() ||
        !Scope->Bindings.IsEmpty() ||
        !Scope->Subscriptions.IsEmpty() ||
        !Scope->Touches.IsEmpty();

    if (!bNeedsMaintenance) { return; }

    // 这里只回收失效弱Owner，不轮询真实输入；4Hz足够且避免每帧容器扫描。
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &ThisClass::Tick),
        InputMaintenanceIntervalSeconds);
}

bool UGamePlatformInputLocalPlayerSubsystem::Tick(float)
{
    check(IsInGameThread());
    TickerHandle.Reset();
    if (!Scope) { return false; }

    const double TickStartSeconds = FPlatformTime::Seconds();
    ++Scope->TotalMaintenanceTicks;

    TArray<FGamePlatformInputContextHandle> ExpiredContexts;
    TArray<FGamePlatformInputBlockHandle> ExpiredBlocks;
    TArray<FGamePlatformInputBindingHandle> ExpiredBindings;
    TArray<FGamePlatformInputTouchHandle> ExpiredTouches;

    for (const auto& Pair : Scope->ContextLeases)
    {
        if (!Pair.Value.Owner.IsValid()) { ExpiredContexts.Add(Pair.Value.Handle); }
    }
    for (const auto& Pair : Scope->Blocks)
    {
        if (!Pair.Value.Owner.IsValid()) { ExpiredBlocks.Add(Pair.Value.Handle); }
    }
    for (const auto& Pair : Scope->Bindings)
    {
        if (!Pair.Value.Component.IsValid()) { ExpiredBindings.Add(Pair.Value.Handle); }
    }
    for (const auto& Pair : Scope->Touches)
    {
        if (!Pair.Value.Owner.IsValid()) { ExpiredTouches.Add(Pair.Value.Handle); }
    }

    for (const auto& Handle : ExpiredTouches) { EndTouchInput(Handle); }
    for (const auto& Handle : ExpiredBindings) { UnbindInputReceiver(Handle); }
    for (const auto& Handle : ExpiredBlocks) { ReleaseInputBlock(Handle); }
    for (const auto& Handle : ExpiredContexts) { ReleaseInputContext(Handle); }
    Scope->TotalExpiredOwnersCollected +=
        ExpiredTouches.Num() + ExpiredBindings.Num() + ExpiredBlocks.Num() + ExpiredContexts.Num();

    for (auto It = Scope->Subscriptions.CreateIterator(); It; ++It)
    {
        if (!It.Value().Owner.IsValid())
        {
            ++Scope->TotalExpiredOwnersCollected;
            It.RemoveCurrent();
        }
    }

    const double TickElapsedMilliseconds = (FPlatformTime::Seconds() - TickStartSeconds) * 1000.0;
    Scope->LastMaintenanceMilliseconds = TickElapsedMilliseconds;
    Scope->MaxMaintenanceMilliseconds = FMath::Max(Scope->MaxMaintenanceMilliseconds, TickElapsedMilliseconds);

    ScheduleMaintenance();
    return false;
}

FGamePlatformInputProfileHandle UGamePlatformInputLocalPlayerSubsystem::PrepareInputProfile(
    const FPrimaryAssetId& ProfileId,
    const FString& LocalSettingsKey,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());

    if (!CanMutate() || IsRunningCommandlet())
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputBusy"), TEXT("输入服务当前不可修改。"));
        return {};
    }
    if (!ProfileId.IsValid() || ProfileId.PrimaryAssetType != UGamePlatformPrimaryDataAsset::DefinitionAssetType() ||
        !IsSafeSettingsKey(LocalSettingsKey))
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidInputProfileRequest"), TEXT("输入Profile身份或本地设置键非法。"));
        return {};
    }

    if (Scope->ProfileHandle.IsValid())
    {
        const FGamePlatformResult ReleaseResult = ReleaseInputProfile(Scope->ProfileHandle);
        if (!ReleaseResult.IsSuccess())
        {
            OutResult = ReleaseResult;
            return {};
        }
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UGameInstance* Instance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputDataUnavailable"), TEXT("当前本地玩家缺少平台数据服务。"));
        return {};
    }

    if (Scope->NextGeneration == MAX_uint64)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputGenerationExhausted"), TEXT("输入配置代次已耗尽。"));
        return {};
    }

    const uint64 Generation = ++Scope->NextGeneration;
    FGamePlatformInputProfileHandle Handle;
    Handle.ScopeId = Scope->ScopeId;
    Handle.Id = FGuid::NewGuid();
    Handle.Generation = Generation;

    const FString Namespace = LocalPlayer && LocalPlayer->GetWorld() && LocalPlayer->GetWorld()->WorldType == EWorldType::PIE
        ? TEXT("PIE") : TEXT("Game");
    const int32 PlayerIndex = LocalPlayer ? FMath::Max(0, LocalPlayer->GetControllerId()) : 0;
    const std::string StableKey = Policy::StableSettingsKey(
        TCHAR_TO_UTF8(*Namespace),
        TCHAR_TO_UTF8(*LocalSettingsKey),
        PlayerIndex,
        TCHAR_TO_UTF8(*ProfileId.PrimaryAssetName.ToString()));
    if (StableKey.empty())
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputSettingsKeyInvalid"), TEXT("无法构造稳定本地输入设置键。"));
        return {};
    }

    FGamePlatformResult Accepted;
    TWeakObjectPtr<UGamePlatformInputLocalPlayerSubsystem> WeakThis(this);
    const FGamePlatformDataLease Lease = Data->AcquireDefinition(
        ProfileId,
        UGamePlatformInputProfileDefinition::StaticClass(),
        {TEXT("Input")},
        EGamePlatformDataLifetime::Instance,
        LocalPlayer,
        [WeakThis, Generation](const FGamePlatformDataLease&, const FGamePlatformResult& Result)
        {
            if (UGamePlatformInputLocalPlayerSubsystem* Self = WeakThis.Get())
            {
                Self->CompleteProfile(Generation, Result);
            }
        },
        Accepted);

    if (!Accepted.IsSuccess() || !Lease.IsValid())
    {
        OutResult = Accepted.IsSuccess()
            ? FGamePlatformResult::Failure(TEXT("InputProfileLeaseMissing"), TEXT("输入Profile请求未签发有效租约。"))
            : Accepted;
        return {};
    }

    Scope->ProfileHandle = Handle;
    Scope->ProfileLease = Lease;
    // 本地区段名只保存稳定哈希，不把调用方提供的不透明用户键或Profile身份以明文写入INI。
    // MD5仅用于本地稳定命名和降低明文暴露，不作为密码学安全边界或认证凭据。
    const FString StableKeyText = UTF8_TO_TCHAR(StableKey.c_str());
    Scope->SettingsSection = FString::Printf(TEXT("GamePlatformInput.%s"), *FMD5::HashAnsiString(*StableKeyText));
    Scope->bProfilePrepared = false;
    Scope->bMappingsApplied = false;
    Scope->bPreferencesSaved = false;
    Scope->LastResult = FGamePlatformResult::Success();

    OutResult = FGamePlatformResult::Success();
    return Handle;
}

void UGamePlatformInputLocalPlayerSubsystem::CompleteProfile(uint64 Generation, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (!Scope || !Scope->ProfileHandle.IsValid() || Scope->ProfileHandle.Generation != Generation) { return; }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UGameInstance* Instance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Result.IsSuccess() || !Data)
    {
        Scope->LastResult = Result.IsSuccess()
            ? FGamePlatformResult::Failure(TEXT("InputDataUnavailable"), TEXT("Profile完成时数据服务已失效。"))
            : Result;
        return;
    }

    const UGamePlatformInputProfileDefinition* Loaded =
        Cast<UGamePlatformInputProfileDefinition>(Data->GetLoadedDefinition(Scope->ProfileLease));
    if (!Loaded)
    {
        Scope->LastResult = FGamePlatformResult::Failure(TEXT("InputProfileLoadFailed"), TEXT("输入Profile租约完成但定义对象不可读。"));
        return;
    }

    const FGamePlatformResult Validation = Loaded->ValidateDefinition();
    if (!Validation.IsSuccess())
    {
        Scope->LastResult = Validation;
        return;
    }

    Scope->Actions.Reset();
    Scope->Contexts.Reset();

    for (const FGamePlatformInputActionDefinition& Entry : Loaded->Actions)
    {
        UInputAction* Action = Entry.Action.Get();
        if (!Action || Action->ValueType != ExpectedValueType(Entry.Semantic))
        {
            Scope->LastResult = FGamePlatformResult::Failure(
                TEXT("InvalidLoadedInputAction"),
                TEXT("Input Bundle未加载动作或动作值维度与平台语义不匹配。"));
            Scope->Actions.Reset();
            return;
        }
        Scope->Actions.Add(Entry.Semantic, Action);
    }

    for (const FGamePlatformInputContextDefinition& Entry : Loaded->Contexts)
    {
        UInputMappingContext* Context = Entry.Context.Get();
        if (!Context)
        {
            Scope->LastResult = FGamePlatformResult::Failure(
                TEXT("InvalidLoadedInputContext"),
                TEXT("Input Bundle未加载映射上下文。"));
            Scope->Contexts.Reset();
            return;
        }
        Scope->Contexts.Add(Entry.Name, Context);
    }

    Scope->Profile = const_cast<UGamePlatformInputProfileDefinition*>(Loaded);
    Scope->Accessibility = Loaded->DefaultAccessibility;
    // Profile准备后校正当前设备族：移动Touch-only或PC-only配置不会继承一个被禁用的默认设备状态。
    if (!IsDeviceEnabled(*Loaded, Scope->ActiveDeviceFamily))
    {
        SetActiveDeviceFamily(SelectFallbackDeviceFamily(*Loaded), true);
    }

    const FGamePlatformResult PreferenceResult = PreparePreferences();
    if (!PreferenceResult.IsSuccess())
    {
        Scope->LastResult = PreferenceResult;
        return;
    }

    Scope->bProfilePrepared = true;
    Scope->LastResult = FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::PreparePreferences()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UEnhancedInputLocalPlayerSubsystem* Enhanced =
        LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    if (!Enhanced)
    {
        return FGamePlatformResult::Failure(TEXT("EnhancedInputUnavailable"), TEXT("本地玩家缺少Enhanced Input子系统。"));
    }

    Enhanced->InitalizeUserSettings();
    UEnhancedInputUserSettings* Settings = Enhanced->GetUserSettings();
    if (!Settings)
    {
        return FGamePlatformResult::Failure(TEXT("InputUserSettingsUnavailable"), TEXT("Enhanced Input用户设置未启用或初始化失败。"));
    }

    Scope->PreferenceContextsOwned.Reset();
    for (const auto& Pair : Scope->Contexts)
    {
        if (UInputMappingContext* Context = Pair.Value.Get())
        {
            if (!Settings->IsMappingContextRegistered(Context) && Settings->RegisterInputMappingContext(Context))
            {
                Scope->PreferenceContextsOwned.Add(Context);
            }
        }
    }

    if (GConfig && !Scope->SettingsSection.IsEmpty())
    {
        double Value = 0.0;
        bool Flag = false;
        if (GConfig->GetDouble(*Scope->SettingsSection, TEXT("LookSensitivityMultiplier"), Value, GGameUserSettingsIni) &&
            FMath::IsFinite(Value) && Value >= 0.1 && Value <= 5.0)
        {
            Scope->Accessibility.LookSensitivityMultiplier = Value;
        }
        if (GConfig->GetDouble(*Scope->SettingsSection, TEXT("MoveDeadZoneMultiplier"), Value, GGameUserSettingsIni) &&
            FMath::IsFinite(Value) && Value >= 0.5 && Value <= 2.0)
        {
            Scope->Accessibility.MoveDeadZoneMultiplier = Value;
        }
        if (GConfig->GetDouble(*Scope->SettingsSection, TEXT("TouchLookSensitivityMultiplier"), Value, GGameUserSettingsIni) &&
            FMath::IsFinite(Value) && Value >= 0.25 && Value <= 3.0)
        {
            Scope->Accessibility.TouchLookSensitivityMultiplier = Value;
        }
        if (GConfig->GetDouble(*Scope->SettingsSection, TEXT("TouchMoveScale"), Value, GGameUserSettingsIni) &&
            FMath::IsFinite(Value) && Value >= 0.5 && Value <= 1.5)
        {
            Scope->Accessibility.TouchMoveScale = Value;
        }
        if (GConfig->GetBool(*Scope->SettingsSection, TEXT("InvertLookX"), Flag, GGameUserSettingsIni))
        {
            Scope->Accessibility.bInvertLookX = Flag;
        }
        if (GConfig->GetBool(*Scope->SettingsSection, TEXT("InvertLookY"), Flag, GGameUserSettingsIni))
        {
            Scope->Accessibility.bInvertLookY = Flag;
        }
    }

    Settings->ApplySettings();
    RebuildMappings();
    return FGamePlatformResult::Success();
}

void UGamePlatformInputLocalPlayerSubsystem::ReleasePreferences()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UEnhancedInputLocalPlayerSubsystem* Enhanced =
        LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    UEnhancedInputUserSettings* Settings = Enhanced ? Enhanced->GetUserSettings() : nullptr;

    if (Settings)
    {
        for (const TWeakObjectPtr<UInputMappingContext>& ContextPtr : Scope->PreferenceContextsOwned)
        {
            if (const UInputMappingContext* Context = ContextPtr.Get())
            {
                Settings->UnregisterInputMappingContext(Context);
            }
        }
    }
    Scope->PreferenceContextsOwned.Reset();
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::ReleaseInputProfile(const FGamePlatformInputProfileHandle& Handle)
{
    check(IsInGameThread());
    if (!CanMutate() || !Handle.IsValid() || !Scope->ProfileHandle.IsValid() ||
        !(static_cast<const FGamePlatformInputIdentity&>(Handle) == static_cast<const FGamePlatformInputIdentity&>(Scope->ProfileHandle)))
    {
        return FGamePlatformResult::Failure(TEXT("StaleInputProfile"), TEXT("输入Profile句柄不属于当前本地玩家配置。"));
    }

    Interrupt(AllInputChannels, EGamePlatformInputEndReason::ProfileReleased);

    TArray<FGamePlatformInputBindingHandle> BindingHandles;
    for (const auto& Pair : Scope->Bindings) { BindingHandles.Add(Pair.Value.Handle); }
    for (const auto& BindingHandle : BindingHandles) { UnbindInputReceiver(BindingHandle); }

    TArray<FGamePlatformInputContextHandle> ContextHandles;
    for (const auto& Pair : Scope->ContextLeases) { ContextHandles.Add(Pair.Value.Handle); }
    for (const auto& ContextHandle : ContextHandles) { ReleaseInputContext(ContextHandle); }

    TArray<FGamePlatformInputTouchHandle> TouchHandles;
    for (const auto& Pair : Scope->Touches) { TouchHandles.Add(Pair.Value.Handle); }
    for (const auto& TouchHandle : TouchHandles) { EndTouchInput(TouchHandle); }

    ReleasePreferences();

    if (UGameInstance* Instance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr)
    {
        if (IGamePlatformDataService* Data = IGamePlatformDataService::Get(*Instance))
        {
            Data->ReleaseDefinition(Scope->ProfileLease);
        }
    }

    Scope->ProfileHandle = {};
    Scope->ProfileLease = {};
    Scope->Profile.Reset();
    Scope->Actions.Reset();
    Scope->Contexts.Reset();
    Scope->SettingsSection.Reset();
    Scope->bProfilePrepared = false;
    Scope->bMappingsApplied = false;
    Scope->bPreferencesSaved = false;
    Scope->LastResult = FGamePlatformResult::Success();
    return FGamePlatformResult::Success();
}

FGamePlatformInputContextHandle UGamePlatformInputLocalPlayerSubsystem::AcquireInputContext(
    FName ContextName,
    int32 Priority,
    TWeakObjectPtr<UObject> Owner,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!CanMutate() || !Scope->bProfilePrepared || !IsCurrentOwner(Owner) || ContextName.IsNone() ||
        Priority < 0 || Priority > 100)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidInputContextRequest"), TEXT("输入上下文请求未通过作用域/Profile/优先级校验。"));
        return {};
    }

    UGamePlatformInputProfileDefinition* Profile = Scope->Profile.Get();
    UInputMappingContext* Context = Scope->Contexts.FindRef(ContextName).Get();
    const FGamePlatformInputContextDefinition* Definition = Profile ? FindContextDefinition(*Profile, ContextName) : nullptr;
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UEnhancedInputLocalPlayerSubsystem* Enhanced =
        LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;

    if (!Context || !Definition || !Enhanced)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputContextUnavailable"), TEXT("输入上下文未准备或Enhanced Input不可用。"));
        return {};
    }

    const std::string PolicyName = ToPolicyName(ContextName);
    const bool bAlreadyOwned = Scope->ContextLedger.EffectivePriority(PolicyName).has_value();
    if (bAlreadyOwned && !Definition->bAllowSharing)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputContextNotShareable"), TEXT("该输入上下文不允许多个平台租约共享。"));
        return {};
    }

    int32 ExistingPriority = 0;
    const bool bAlreadyInEnhanced = Enhanced->HasMappingContext(Context, ExistingPriority);
    if (bAlreadyInEnhanced && !bAlreadyOwned)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("ExternalInputContextConflict"), TEXT("同一映射上下文已由插件外部所有者激活，平台不会接管。"));
        return {};
    }

    if (Scope->NextGeneration == MAX_uint64)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputGenerationExhausted"), TEXT("输入租约代次已耗尽。"));
        return {};
    }

    FGamePlatformInputContextHandle Handle;
    Handle.ScopeId = Scope->ScopeId;
    Handle.Id = FGuid::NewGuid();
    Handle.Generation = ++Scope->NextGeneration;

    const std::optional<int> PreviousPriority = Scope->ContextLedger.EffectivePriority(PolicyName);
    if (!Scope->ContextLedger.Acquire(PolicyToken(Handle), PolicyName, Priority, false))
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputContextCapacity"), TEXT("输入上下文租约容量已满或身份重复。"));
        return {};
    }

    FInputContextRecord Record;
    Record.Handle = Handle;
    Record.ContextName = ContextName;
    Record.Priority = Priority;
    Record.Owner = Owner;
    Scope->ContextLeases.Add(Handle.Id, Record);

    const int32 EffectivePriority = Scope->ContextLedger.EffectivePriority(PolicyName).value_or(Priority);
    if (!PreviousPriority || *PreviousPriority != EffectivePriority)
    {
        TGuardValue<bool> Guard(Scope->bAdjustingContexts, true);
        if (PreviousPriority) { Enhanced->RemoveMappingContext(Context); }
        Enhanced->AddMappingContext(Context, EffectivePriority);
    }

    Scope->bMappingsApplied = false;
    Scope->bPreferencesSaved = false;
    ScheduleMaintenance();
    OutResult = FGamePlatformResult::Success();
    return Handle;
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::ReleaseInputContext(const FGamePlatformInputContextHandle& Handle)
{
    check(IsInGameThread());
    if (!CanMutate() || !Handle.IsValid() || Handle.ScopeId != Scope->ScopeId)
    {
        return FGamePlatformResult::Failure(TEXT("StaleInputContext"), TEXT("输入上下文句柄无效或跨LocalPlayer。"));
    }

    FInputContextRecord* Record = Scope->ContextLeases.Find(Handle.Id);
    if (!Record || !(static_cast<const FGamePlatformInputIdentity&>(Record->Handle) == static_cast<const FGamePlatformInputIdentity&>(Handle)))
    {
        return FGamePlatformResult::Failure(TEXT("StaleInputContext"), TEXT("输入上下文句柄不是当前签发记录。"));
    }

    UInputMappingContext* Context = Scope->Contexts.FindRef(Record->ContextName).Get();
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UEnhancedInputLocalPlayerSubsystem* Enhanced =
        LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;

    const std::string PolicyName = ToPolicyName(Record->ContextName);
    const std::optional<int> PreviousPriority = Scope->ContextLedger.EffectivePriority(PolicyName);
    Scope->ContextLedger.Release(PolicyToken(Record->Handle));
    const std::optional<int> NewPriority = Scope->ContextLedger.EffectivePriority(PolicyName);
    Scope->ContextLeases.Remove(Handle.Id);

    if (Context && Enhanced && PreviousPriority != NewPriority)
    {
        TGuardValue<bool> Guard(Scope->bAdjustingContexts, true);
        Enhanced->RemoveMappingContext(Context);
        if (NewPriority) { Enhanced->AddMappingContext(Context, *NewPriority); }
    }

    Scope->bMappingsApplied = false;
    return FGamePlatformResult::Success();
}

FGamePlatformInputBindingHandle UGamePlatformInputLocalPlayerSubsystem::BindInputReceiver(
    UEnhancedInputComponent& Component,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!CanMutate() || !Scope->bProfilePrepared)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputProfileNotReady"), TEXT("输入Profile尚未准备完成。"));
        return {};
    }

    APlayerController* Controller = GetLocalPlayer() && GetWorld()
        ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    const bool bControllerOwned = Controller && Component.GetTypedOuter<APlayerController>() == Controller;
    const bool bPawnOwned = Pawn && Component.GetTypedOuter<APawn>() == Pawn;
    if (!Controller || (!bControllerOwned && !bPawnOwned))
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputReceiverScopeMismatch"), TEXT("绑定组件不属于当前本地控制器或Pawn。"));
        return {};
    }
    if (Scope->Bindings.Num() >= MaxInputBindings)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputBindingCapacity"), TEXT("本地玩家输入绑定数量已达到安全上限。"));
        return {};
    }

    for (const auto& Pair : Scope->Bindings)
    {
        if (Pair.Value.Component.Get() == &Component)
        {
            OutResult = FGamePlatformResult::Success();
            return Pair.Value.Handle;
        }
    }

    if (Scope->NextGeneration == MAX_uint64)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputGenerationExhausted"), TEXT("输入绑定代次已耗尽。"));
        return {};
    }

    FInputBindingRecord Record;
    Record.Handle.ScopeId = Scope->ScopeId;
    Record.Handle.Id = FGuid::NewGuid();
    Record.Handle.Generation = ++Scope->NextGeneration;
    Record.Component = &Component;
    const uint64 BindingGeneration = ++Scope->BindingGeneration;

    TWeakObjectPtr<UGamePlatformInputLocalPlayerSubsystem> WeakThis(this);
    const ETriggerEvent Phases[] =
    {
        ETriggerEvent::Started,
        ETriggerEvent::Ongoing,
        ETriggerEvent::Triggered,
        ETriggerEvent::Completed,
        ETriggerEvent::Canceled
    };

    for (const auto& Pair : Scope->Actions)
    {
        const EGamePlatformInputSemantic Semantic = Pair.Key;
        const UInputAction* Action = Pair.Value.Get();
        if (!Action) { continue; }

        for (const ETriggerEvent Phase : Phases)
        {
            FEnhancedInputActionEventBinding& Binding = Component.BindActionValueLambda(
                Action,
                Phase,
                [WeakThis, Semantic, Phase, BindingGeneration](const FInputActionValue& Value)
                {
                    if (UGamePlatformInputLocalPlayerSubsystem* Self = WeakThis.Get())
                    {
                        Self->HandleBoundInput(Value, Semantic, Phase, BindingGeneration);
                    }
                });
            Record.NativeBindingHandles.Add(Binding.GetHandle());
        }
    }

    const FGamePlatformInputBindingHandle Handle = Record.Handle;
    Scope->Bindings.Add(Handle.Id, MoveTemp(Record));
    ScheduleMaintenance();
    OutResult = FGamePlatformResult::Success();
    return Handle;
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::UnbindInputReceiver(const FGamePlatformInputBindingHandle& Handle)
{
    check(IsInGameThread());
    if (!CanMutate() || !Handle.IsValid() || Handle.ScopeId != Scope->ScopeId)
    {
        return FGamePlatformResult::Failure(TEXT("StaleInputBinding"), TEXT("输入绑定句柄无效、跨LocalPlayer或当前正在分发事件。"));
    }

    FInputBindingRecord* Record = Scope->Bindings.Find(Handle.Id);
    if (!Record || !(static_cast<const FGamePlatformInputIdentity&>(Record->Handle) == static_cast<const FGamePlatformInputIdentity&>(Handle)))
    {
        return FGamePlatformResult::Failure(TEXT("StaleInputBinding"), TEXT("输入绑定句柄不是当前签发记录。"));
    }

    if (UEnhancedInputComponent* Component = Record->Component.Get())
    {
        for (const uint32 NativeHandle : Record->NativeBindingHandles)
        {
            Component->RemoveBindingByHandle(NativeHandle);
        }
    }

    Scope->Bindings.Remove(Handle.Id);
    Interrupt(
        static_cast<uint8>(EGamePlatformInputChannel::Move) |
        static_cast<uint8>(EGamePlatformInputChannel::Look) |
        static_cast<uint8>(EGamePlatformInputChannel::Actions),
        EGamePlatformInputEndReason::ReceiverChanged);
    return FGamePlatformResult::Success();
}

FGamePlatformInputBlockHandle UGamePlatformInputLocalPlayerSubsystem::AcquireInputBlock(
    uint8 Channels,
    FName Reason,
    TWeakObjectPtr<UObject> Owner,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!CanMutate() || !IsCurrentOwner(Owner) || Reason.IsNone() || Channels == 0 || (Channels & ~AllInputChannels) != 0)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidInputBlock"), TEXT("输入阻断通道、原因或Owner非法。"));
        return {};
    }

    if (Scope->NextGeneration == MAX_uint64)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputGenerationExhausted"), TEXT("输入阻断代次已耗尽。"));
        return {};
    }

    FGamePlatformInputBlockHandle Handle;
    Handle.ScopeId = Scope->ScopeId;
    Handle.Id = FGuid::NewGuid();
    Handle.Generation = ++Scope->NextGeneration;

    if (!Scope->BlockLedger.Acquire(PolicyToken(Handle), Channels))
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputBlockCapacity"), TEXT("输入阻断租约容量已满或身份重复。"));
        return {};
    }

    FInputBlockRecord Record;
    Record.Handle = Handle;
    Record.Channels = Channels;
    Record.Reason = Reason;
    Record.Owner = Owner;
    Scope->Blocks.Add(Handle.Id, Record);

    Interrupt(Channels, EGamePlatformInputEndReason::Blocked);
    ScheduleMaintenance();
    OutResult = FGamePlatformResult::Success();
    return Handle;
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::ReleaseInputBlock(const FGamePlatformInputBlockHandle& Handle)
{
    check(IsInGameThread());
    if (!CanMutate() || !Handle.IsValid() || Handle.ScopeId != Scope->ScopeId)
    {
        return FGamePlatformResult::Failure(TEXT("StaleInputBlock"), TEXT("输入阻断句柄无效、跨LocalPlayer或当前正在分发事件。"));
    }

    FInputBlockRecord* Record = Scope->Blocks.Find(Handle.Id);
    if (!Record || !(static_cast<const FGamePlatformInputIdentity&>(Record->Handle) == static_cast<const FGamePlatformInputIdentity&>(Handle)))
    {
        return FGamePlatformResult::Failure(TEXT("StaleInputBlock"), TEXT("输入阻断句柄不是当前签发记录。"));
    }

    Scope->BlockLedger.Release(PolicyToken(Record->Handle));
    Scope->Blocks.Remove(Handle.Id);
    return FGamePlatformResult::Success();
}

void UGamePlatformInputLocalPlayerSubsystem::SetApplicationFocus(bool bHasFocus)
{
    check(IsInGameThread());
    if (!CanMutate() || Scope->bHasFocus == bHasFocus) { return; }

    Scope->bHasFocus = bHasFocus;
    if (!bHasFocus)
    {
        Interrupt(AllInputChannels, EGamePlatformInputEndReason::FocusLost);
        if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Enhanced = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (UEnhancedPlayerInput* PlayerInput = Enhanced->GetPlayerInput())
                {
                    PlayerInput->FlushPressedKeys();
                }
            }
        }
    }
}

FGamePlatformInputSubscription UGamePlatformInputLocalPlayerSubsystem::SubscribeInputEvents(
    TWeakObjectPtr<UObject> Owner,
    TFunction<void(const FGamePlatformInputEvent&)> Callback)
{
    check(IsInGameThread());
    if (!CanMutate() || !IsCurrentOwner(Owner) || !Callback || Scope->Subscriptions.Num() >= MaxInputSubscriptions ||
        Scope->NextGeneration == MAX_uint64)
    {
        return {};
    }

    FInputSubscriptionRecord Record;
    Record.Handle.ScopeId = Scope->ScopeId;
    Record.Handle.Id = FGuid::NewGuid();
    Record.Handle.Generation = ++Scope->NextGeneration;
    Record.Owner = Owner;
    Record.Callback = MoveTemp(Callback);

    const FGamePlatformInputSubscription Handle = Record.Handle;
    Scope->Subscriptions.Add(Handle.Id, MoveTemp(Record));
    ScheduleMaintenance();
    return Handle;
}

bool UGamePlatformInputLocalPlayerSubsystem::UnsubscribeInputEvents(const FGamePlatformInputSubscription& Handle)
{
    check(IsInGameThread());
    if (!CanMutate() || !Handle.IsValid() || Handle.ScopeId != Scope->ScopeId) { return false; }

    const FInputSubscriptionRecord* Record = Scope->Subscriptions.Find(Handle.Id);
    if (!Record || !(static_cast<const FGamePlatformInputIdentity&>(Record->Handle) == static_cast<const FGamePlatformInputIdentity&>(Handle)))
    {
        return false;
    }

    return Scope->Subscriptions.Remove(Handle.Id) == 1;
}

FGamePlatformInputSnapshot UGamePlatformInputLocalPlayerSubsystem::GetInputSnapshot() const
{
    check(IsInGameThread());
    FGamePlatformInputSnapshot Snapshot;
    if (!Scope) { return Snapshot; }

    Snapshot.bProfilePrepared = Scope->bProfilePrepared;
    Snapshot.bBindingsReady = !Scope->Bindings.IsEmpty();
    Snapshot.bMappingsApplied = Scope->bMappingsApplied;
    // 只有Profile、真实Receiver绑定和焦点同时存在，且玩法通道未被阻断，才声明玩法输入可用。
    Snapshot.bGameplayInputEnabled =
        Scope->bProfilePrepared &&
        !Scope->Bindings.IsEmpty() &&
        Scope->bHasFocus &&
        !Scope->BlockLedger.IsBlocked(
            static_cast<uint8>(EGamePlatformInputChannel::Move) |
            static_cast<uint8>(EGamePlatformInputChannel::Look) |
            static_cast<uint8>(EGamePlatformInputChannel::Actions));
    Snapshot.bPreferencesSaved = Scope->bPreferencesSaved;
    Snapshot.ActiveDeviceFamily = Scope->ActiveDeviceFamily;
    Snapshot.Accessibility = Scope->Accessibility;
    Snapshot.ProfileGeneration = Scope->ProfileHandle.Generation;
    Snapshot.BindingGeneration = Scope->BindingGeneration;
    Snapshot.SettingsRevision = Scope->SettingsRevision;
    Snapshot.DeviceRevision = Scope->DeviceRevision;
    Snapshot.BlockedChannels = static_cast<uint8>(Scope->BlockLedger.CombinedMask());
    Snapshot.ContextLeaseCount = Scope->ContextLeases.Num();
    Snapshot.OwnedBindingCount = Scope->Bindings.Num();
    Snapshot.TouchSourceCount = Scope->Touches.Num();
    Snapshot.Result = Scope->LastResult;
    return Snapshot;
}
FGamePlatformInputDiagnostics UGamePlatformInputLocalPlayerSubsystem::GetInputDiagnostics() const
{
    check(IsInGameThread());
    FGamePlatformInputDiagnostics Diagnostics;
    if (!Scope) { return Diagnostics; }

    Diagnostics.ScopeId = Scope->ScopeId;
    Diagnostics.bMaintenanceTickerScheduled = TickerHandle.IsValid();
    Diagnostics.ActiveDeviceFamily = Scope->ActiveDeviceFamily;
    Diagnostics.DeviceRevision = Scope->DeviceRevision;
    Diagnostics.ContextLeaseCount = Scope->ContextLeases.Num();
    Diagnostics.BlockLeaseCount = Scope->Blocks.Num();
    Diagnostics.BindingCount = Scope->Bindings.Num();
    Diagnostics.SubscriptionCount = Scope->Subscriptions.Num();
    Diagnostics.TouchSourceCount = Scope->Touches.Num();
    Diagnostics.TotalInputEventsPublished = Scope->TotalInputEventsPublished;
    Diagnostics.TotalSubscriberCallbacks = Scope->TotalSubscriberCallbacks;
    Diagnostics.TotalDeviceFamilyChanges = Scope->TotalDeviceFamilyChanges;
    Diagnostics.TotalMappingRebuildRequests = Scope->TotalMappingRebuildRequests;
    Diagnostics.TotalMaintenanceTicks = Scope->TotalMaintenanceTicks;
    Diagnostics.TotalExpiredOwnersCollected = Scope->TotalExpiredOwnersCollected;
    Diagnostics.LastMaintenanceMilliseconds = Scope->LastMaintenanceMilliseconds;
    Diagnostics.MaxMaintenanceMilliseconds = Scope->MaxMaintenanceMilliseconds;
    return Diagnostics;
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::NotifyInputDeviceActivity(EGamePlatformInputDeviceFamily DeviceFamily)
{
    check(IsInGameThread());
    if (!CanMutate() || DeviceFamily == EGamePlatformInputDeviceFamily::Unknown)
    {
        return FGamePlatformResult::Failure(TEXT("InvalidInputDevice"), TEXT("输入设备族不能为空。"));
    }

    if (UGamePlatformInputProfileDefinition* Profile = Scope->Profile.Get())
    {
        if (!IsDeviceEnabled(*Profile, DeviceFamily))
        {
            return FGamePlatformResult::Failure(TEXT("InputDeviceDisabled"), TEXT("当前输入Profile未启用该设备族。"));
        }
    }

    // 设备切换只更新本地提示状态和修订号，不重建Mapping、不中断当前动作，避免热切换抖动。
    SetActiveDeviceFamily(DeviceFamily, true);
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::SetAccessibilitySettings(
    const FGamePlatformInputAccessibilitySettings& Settings)
{
    check(IsInGameThread());
    if (!CanMutate() || !FMath::IsFinite(Settings.LookSensitivityMultiplier) ||
        Settings.LookSensitivityMultiplier < 0.1 || Settings.LookSensitivityMultiplier > 5.0 ||
        !FMath::IsFinite(Settings.MoveDeadZoneMultiplier) ||
        Settings.MoveDeadZoneMultiplier < 0.5 || Settings.MoveDeadZoneMultiplier > 2.0 ||
        !FMath::IsFinite(Settings.TouchLookSensitivityMultiplier) ||
        Settings.TouchLookSensitivityMultiplier < 0.25 || Settings.TouchLookSensitivityMultiplier > 3.0 ||
        !FMath::IsFinite(Settings.TouchMoveScale) ||
        Settings.TouchMoveScale < 0.5 || Settings.TouchMoveScale > 1.5)
    {
        return FGamePlatformResult::Failure(TEXT("InvalidInputAccessibility"), TEXT("输入无障碍/舒适度参数超出安全范围。"));
    }

    Scope->Accessibility = Settings;
    Scope->bPreferencesSaved = false;
    return FGamePlatformResult::Success();
}

FGamePlatformInputAccessibilitySettings UGamePlatformInputLocalPlayerSubsystem::GetAccessibilitySettings() const
{
    check(IsInGameThread());
    return Scope ? Scope->Accessibility : FGamePlatformInputAccessibilitySettings{};
}

TArray<FGamePlatformInputMapping> UGamePlatformInputLocalPlayerSubsystem::ListPlayerMappings() const
{
    check(IsInGameThread());
    TArray<FGamePlatformInputMapping> Result;
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UEnhancedInputLocalPlayerSubsystem* Enhanced =
        LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    UEnhancedInputUserSettings* Settings = Enhanced ? Enhanced->GetUserSettings() : nullptr;
    UEnhancedPlayerMappableKeyProfile* KeyProfile = Settings ? Settings->GetActiveKeyProfile() : nullptr;
    if (!KeyProfile) { return Result; }

    for (const auto& RowPair : KeyProfile->GetPlayerMappingRows())
    {
        for (const FPlayerKeyMapping& Mapping : RowPair.Value.Mappings)
        {
            if (!Mapping.IsValid()) { continue; }
            FGamePlatformInputMapping Item;
            Item.RowName = Mapping.GetMappingName();
            Item.Slot = static_cast<int32>(Mapping.GetSlot());
            Item.Key = Mapping.GetCurrentKey();
            Item.DefaultKey = Mapping.GetDefaultKey();
            Item.bGamepad = Item.Key.IsGamepadKey();
            Result.Add(MoveTemp(Item));
        }
    }

    Result.Sort([](const FGamePlatformInputMapping& A, const FGamePlatformInputMapping& B)
    {
        const int32 NameCompare = A.RowName.Compare(B.RowName);
        return NameCompare == 0 ? A.Slot < B.Slot : NameCompare < 0;
    });
    return Result;
}

FGamePlatformInputRebindPreview UGamePlatformInputLocalPlayerSubsystem::PreviewRebind(
    FName RowName,
    int32 Slot,
    FKey Key) const
{
    check(IsInGameThread());
    FGamePlatformInputRebindPreview Preview;
    if (!Scope || !Scope->bProfilePrepared || RowName.IsNone() || !IsValidMappingSlot(Slot) || !IsSupportedRebindKey(Key))
    {
        Preview.Result = FGamePlatformResult::Failure(TEXT("InvalidRebind"), TEXT("重绑定行、槽或按键不在支持范围。"));
        return Preview;
    }

    bool bRowExists = false;
    const bool bGamepad = Key.IsGamepadKey();
    for (const FGamePlatformInputMapping& Mapping : ListPlayerMappings())
    {
        if (Mapping.RowName == RowName) { bRowExists = true; }
        if (Mapping.RowName != RowName && Mapping.Key == Key && Mapping.bGamepad == bGamepad)
        {
            Preview.ConflictingRows.AddUnique(Mapping.RowName);
        }
    }

    if (!bRowExists)
    {
        Preview.Result = FGamePlatformResult::Failure(TEXT("RebindRowMissing"), TEXT("重绑定行未在当前用户设置中登记。"));
    }
    else if (!Preview.ConflictingRows.IsEmpty())
    {
        Preview.Result = FGamePlatformResult::Failure(TEXT("RebindConflict"), TEXT("目标按键已被同设备类型的其他映射占用。"));
    }
    else
    {
        Preview.Result = FGamePlatformResult::Success();
    }
    return Preview;
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::ApplyRebind(FName RowName, int32 Slot, FKey Key)
{
    check(IsInGameThread());
    if (!CanMutate())
    {
        return FGamePlatformResult::Failure(TEXT("InputBusy"), TEXT("输入服务正在分发事件，不能重绑定。"));
    }

    const FGamePlatformInputRebindPreview Preview = PreviewRebind(RowName, Slot, Key);
    if (!Preview.Result.IsSuccess()) { return Preview.Result; }

    UEnhancedInputLocalPlayerSubsystem* Enhanced =
        GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    UEnhancedInputUserSettings* Settings = Enhanced ? Enhanced->GetUserSettings() : nullptr;
    if (!Settings)
    {
        return FGamePlatformResult::Failure(TEXT("InputUserSettingsUnavailable"), TEXT("Enhanced Input用户设置不可用。"));
    }

    FMapPlayerKeyArgs Args;
    Args.MappingName = RowName;
    Args.Slot = static_cast<EPlayerMappableKeySlot>(Slot);
    Args.NewKey = Key;

    FGameplayTagContainer FailureReason;
    Settings->MapPlayerKey(Args, FailureReason);
    if (!FailureReason.IsEmpty())
    {
        return FGamePlatformResult::Failure(TEXT("RebindRejected"), TEXT("Enhanced Input拒绝重绑定请求。"));
    }

    Settings->ApplySettings();
    Scope->bPreferencesSaved = false;
    RebuildMappings();
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::ResetMappings(FName RowName)
{
    check(IsInGameThread());
    if (!CanMutate() || !Scope->bProfilePrepared)
    {
        return FGamePlatformResult::Failure(TEXT("InputProfileNotReady"), TEXT("输入Profile未准备或服务忙。"));
    }

    UEnhancedInputLocalPlayerSubsystem* Enhanced =
        GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    UEnhancedInputUserSettings* Settings = Enhanced ? Enhanced->GetUserSettings() : nullptr;
    UEnhancedPlayerMappableKeyProfile* KeyProfile = Settings ? Settings->GetActiveKeyProfile() : nullptr;
    if (!Settings || !KeyProfile)
    {
        return FGamePlatformResult::Failure(TEXT("InputUserSettingsUnavailable"), TEXT("Enhanced Input用户设置不可用。"));
    }

    TArray<FName> Rows;
    if (RowName.IsNone())
    {
        KeyProfile->GetPlayerMappingRows().GetKeys(Rows);
    }
    else
    {
        if (!KeyProfile->GetPlayerMappingRows().Contains(RowName))
        {
            return FGamePlatformResult::Failure(TEXT("RebindRowMissing"), TEXT("待重置的映射行不存在。"));
        }
        Rows.Add(RowName);
    }

    for (const FName Row : Rows)
    {
        FMapPlayerKeyArgs Args;
        Args.MappingName = Row;
        Args.Slot = EPlayerMappableKeySlot::Unspecified;
        FGameplayTagContainer FailureReason;
        Settings->ResetAllPlayerKeysInRow(Args, FailureReason);
        if (!FailureReason.IsEmpty())
        {
            return FGamePlatformResult::Failure(TEXT("ResetMappingRejected"), TEXT("Enhanced Input拒绝映射重置请求。"));
        }
    }

    Settings->ApplySettings();
    Scope->bPreferencesSaved = false;
    RebuildMappings();
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::SaveInputPreferences()
{
    check(IsInGameThread());
    if (!Scope || !Scope->bProfilePrepared)
    {
        return FGamePlatformResult::Failure(TEXT("InputProfileNotReady"), TEXT("输入Profile未准备完成。"));
    }

    UEnhancedInputLocalPlayerSubsystem* Enhanced =
        GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    UEnhancedInputUserSettings* Settings = Enhanced ? Enhanced->GetUserSettings() : nullptr;
    if (!Settings || !GConfig || Scope->SettingsSection.IsEmpty())
    {
        return FGamePlatformResult::Failure(TEXT("InputPreferencesUnavailable"), TEXT("输入偏好持久化服务不可用。"));
    }

    // 磁盘写入只发生在显式Save调用，不进入每帧输入路径。
    Settings->SaveSettings();
    GConfig->SetDouble(*Scope->SettingsSection, TEXT("LookSensitivityMultiplier"), Scope->Accessibility.LookSensitivityMultiplier, GGameUserSettingsIni);
    GConfig->SetDouble(*Scope->SettingsSection, TEXT("MoveDeadZoneMultiplier"), Scope->Accessibility.MoveDeadZoneMultiplier, GGameUserSettingsIni);
    GConfig->SetDouble(*Scope->SettingsSection, TEXT("TouchLookSensitivityMultiplier"), Scope->Accessibility.TouchLookSensitivityMultiplier, GGameUserSettingsIni);
    GConfig->SetDouble(*Scope->SettingsSection, TEXT("TouchMoveScale"), Scope->Accessibility.TouchMoveScale, GGameUserSettingsIni);
    GConfig->SetBool(*Scope->SettingsSection, TEXT("InvertLookX"), Scope->Accessibility.bInvertLookX, GGameUserSettingsIni);
    GConfig->SetBool(*Scope->SettingsSection, TEXT("InvertLookY"), Scope->Accessibility.bInvertLookY, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);

    Scope->bPreferencesSaved = true;
    ++Scope->SettingsRevision;
    return FGamePlatformResult::Success();
}

FGamePlatformInputTouchHandle UGamePlatformInputLocalPlayerSubsystem::BeginTouchInput(
    int32 PointerId,
    EGamePlatformInputSemantic Semantic,
    TWeakObjectPtr<UObject> Owner,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    UGamePlatformInputProfileDefinition* Profile = Scope ? Scope->Profile.Get() : nullptr;
    if (!CanMutate() || !Scope->bProfilePrepared || !Profile || !Profile->bEnableTouch ||
        PointerId < MinTouchPointerId || PointerId > MaxTouchPointerId || !IsCurrentOwner(Owner) ||
        !Scope->Actions.Contains(Semantic))
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InvalidTouchInput"), TEXT("Touch输入未通过Profile、指针、Owner或语义校验。"));
        return {};
    }

    for (const auto& Pair : Scope->Touches)
    {
        if (Pair.Value.PointerId == PointerId || Pair.Value.Semantic == Semantic)
        {
            OutResult = FGamePlatformResult::Failure(TEXT("TouchInputConflict"), TEXT("同一触点或语义已有活动Touch来源。"));
            return {};
        }
    }

    if (Scope->NextGeneration == MAX_uint64)
    {
        OutResult = FGamePlatformResult::Failure(TEXT("InputGenerationExhausted"), TEXT("Touch输入代次已耗尽。"));
        return {};
    }

    FInputTouchRecord Record;
    Record.Handle.ScopeId = Scope->ScopeId;
    Record.Handle.Id = FGuid::NewGuid();
    Record.Handle.Generation = ++Scope->NextGeneration;
    Record.PointerId = PointerId;
    Record.Semantic = Semantic;
    Record.Owner = Owner;

    const FGamePlatformInputTouchHandle Handle = Record.Handle;
    Scope->Touches.Add(Handle.Id, MoveTemp(Record));
    SetActiveDeviceFamily(EGamePlatformInputDeviceFamily::Touch, true);
    ScheduleMaintenance();
    OutResult = FGamePlatformResult::Success();
    return Handle;
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::UpdateTouchInput(
    const FGamePlatformInputTouchHandle& Handle,
    const FInputActionValue& Value)
{
    check(IsInGameThread());
    if (!CanMutate() || !Handle.IsValid() || Handle.ScopeId != Scope->ScopeId)
    {
        return FGamePlatformResult::Failure(TEXT("StaleTouchInput"), TEXT("Touch输入句柄无效、跨LocalPlayer或当前正在分发事件。"));
    }

    FInputTouchRecord* Record = Scope->Touches.Find(Handle.Id);
    if (!Record || !(static_cast<const FGamePlatformInputIdentity&>(Record->Handle) == static_cast<const FGamePlatformInputIdentity&>(Handle)) ||
        !Record->Owner.IsValid())
    {
        return FGamePlatformResult::Failure(TEXT("StaleTouchInput"), TEXT("Touch输入来源已失效。"));
    }

    UInputAction* Action = Scope->Actions.FindRef(Record->Semantic).Get();
    UEnhancedInputLocalPlayerSubsystem* Enhanced =
        GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    if (!Action || !Enhanced || Value.GetValueType() != Action->ValueType || !FMath::IsFinite(Value.GetMagnitude()))
    {
        return FGamePlatformResult::Failure(TEXT("InvalidTouchValue"), TEXT("Touch值维度、数值或Enhanced Input状态非法。"));
    }

    SetActiveDeviceFamily(EGamePlatformInputDeviceFamily::Touch, true);
    Enhanced->InjectInputForAction(Action, Value, {}, {});
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformInputLocalPlayerSubsystem::EndTouchInput(const FGamePlatformInputTouchHandle& Handle)
{
    check(IsInGameThread());
    if (!CanMutate() || !Handle.IsValid() || Handle.ScopeId != Scope->ScopeId)
    {
        return FGamePlatformResult::Failure(TEXT("StaleTouchInput"), TEXT("Touch输入句柄无效、跨LocalPlayer或当前正在分发事件。"));
    }

    FInputTouchRecord* Record = Scope->Touches.Find(Handle.Id);
    if (!Record || !(static_cast<const FGamePlatformInputIdentity&>(Record->Handle) == static_cast<const FGamePlatformInputIdentity&>(Handle)))
    {
        return FGamePlatformResult::Failure(TEXT("StaleTouchInput"), TEXT("Touch输入句柄不是当前签发记录。"));
    }

    UInputAction* Action = Scope->Actions.FindRef(Record->Semantic).Get();
    if (Action)
    {
        if (UEnhancedInputLocalPlayerSubsystem* Enhanced =
            GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
        {
            Enhanced->InjectInputForAction(Action, MakeNeutralValue(Action->ValueType), {}, {});
        }
    }

    const uint8 Channel = GamePlatformInputServices::GetChannel(Record->Semantic);
    Interrupt(Channel, EGamePlatformInputEndReason::NativeCanceled);
    Scope->Touches.Remove(Handle.Id);
    return FGamePlatformResult::Success();
}

void UGamePlatformInputLocalPlayerSubsystem::HandleBoundInput(
    const FInputActionValue& Value,
    EGamePlatformInputSemantic Semantic,
    ETriggerEvent Phase,
    uint64 BindingGeneration)
{
    Route(Value, Semantic, Phase, BindingGeneration);
}

void UGamePlatformInputLocalPlayerSubsystem::Route(
    const FInputActionValue& Value,
    EGamePlatformInputSemantic Semantic,
    ETriggerEvent Phase,
    uint64 BindingGeneration)
{
    check(IsInGameThread());
    if (!Scope || !Scope->bProfilePrepared || BindingGeneration != Scope->BindingGeneration) { return; }

    UGamePlatformInputProfileDefinition* Profile = Scope->Profile.Get();
    if (!Profile || !IsDeviceEnabled(*Profile, Scope->ActiveDeviceFamily) ||
        Value.GetValueType() != ExpectedValueType(Semantic) || !FMath::IsFinite(Value.GetMagnitude()))
    {
        Interrupt(GamePlatformInputServices::GetChannel(Semantic), EGamePlatformInputEndReason::Blocked);
        return;
    }

    FInputActionValue Routed = Value;
    if (Semantic == EGamePlatformInputSemantic::Move)
    {
        const FVector2D Raw = Value.Get<FVector2D>();
        const double BaseDeadZone = Scope->ActiveDeviceFamily == EGamePlatformInputDeviceFamily::Touch
            ? Profile->TouchAnalogDeadZone : Profile->AnalogDeadZone;
        const double DeadZone = FMath::Clamp(
            BaseDeadZone * Scope->Accessibility.MoveDeadZoneMultiplier,
            0.0,
            0.95);
        const auto Axis = Policy::ApplyRadialDeadZone(Raw.X, Raw.Y, DeadZone);
        const double MoveScale = Scope->ActiveDeviceFamily == EGamePlatformInputDeviceFamily::Touch
            ? Scope->Accessibility.TouchMoveScale : 1.0;
        const auto Scaled = Policy::ApplyAxisScale(Axis.first, Axis.second, MoveScale);
        Routed = FInputActionValue(FVector2D(Scaled.first, Scaled.second));
    }
    else if (Semantic == EGamePlatformInputSemantic::LookDelta)
    {
        const FVector2D Raw = Value.Get<FVector2D>();
        const auto Axis = Policy::ApplyLookPreference(
            Raw.X * Profile->LookDegreesPerCount,
            Raw.Y * Profile->LookDegreesPerCount,
            Policy::CombineSensitivity(
                Scope->Accessibility.LookSensitivityMultiplier,
                Scope->ActiveDeviceFamily == EGamePlatformInputDeviceFamily::Touch
                    ? Scope->Accessibility.TouchLookSensitivityMultiplier : 1.0),
            Scope->Accessibility.bInvertLookX,
            Scope->Accessibility.bInvertLookY);
        Routed = FInputActionValue(FVector2D(Axis.first, Axis.second));
    }
    else if (Semantic == EGamePlatformInputSemantic::LookRate)
    {
        const FVector2D Raw = Value.Get<FVector2D>();
        const double BaseDeadZone = Scope->ActiveDeviceFamily == EGamePlatformInputDeviceFamily::Touch
            ? Profile->TouchAnalogDeadZone : Profile->AnalogDeadZone;
        const auto DeadZoneAxis = Policy::ApplyRadialDeadZone(
            Raw.X,
            Raw.Y,
            FMath::Clamp(BaseDeadZone * Scope->Accessibility.MoveDeadZoneMultiplier, 0.0, 0.95));
        const auto Axis = Policy::ApplyLookPreference(
            DeadZoneAxis.first * Profile->LookDegreesPerSecond,
            DeadZoneAxis.second * Profile->LookDegreesPerSecond,
            Policy::CombineSensitivity(
                Scope->Accessibility.LookSensitivityMultiplier,
                Scope->ActiveDeviceFamily == EGamePlatformInputDeviceFamily::Touch
                    ? Scope->Accessibility.TouchLookSensitivityMultiplier : 1.0),
            Scope->Accessibility.bInvertLookX,
            Scope->Accessibility.bInvertLookY);
        Routed = FInputActionValue(FVector2D(Axis.first, Axis.second));
    }

    const uint8 Channel = GamePlatformInputServices::GetChannel(Semantic);
    const bool bBlocked = !Scope->bHasFocus || Scope->BlockLedger.IsBlocked(Channel);
    const bool bTerminal = Phase == ETriggerEvent::Completed || Phase == ETriggerEvent::Canceled;
    Policy::ActionGate& Gate = Scope->ActionGates[static_cast<std::size_t>(Semantic)];
    if (!Gate.Observe(Routed.GetMagnitude(), bTerminal, bBlocked)) { return; }

    FGamePlatformInputEvent Event;
    Event.Semantic = Semantic;
    Event.Phase = Phase;
    Event.Value = Routed;
    Event.Unit = GamePlatformInputServices::GetUnit(Semantic);
    Event.EndReason = Phase == ETriggerEvent::Canceled
        ? EGamePlatformInputEndReason::NativeCanceled
        : (Phase == ETriggerEvent::Completed ? EGamePlatformInputEndReason::NativeCompleted : EGamePlatformInputEndReason::None);
    Event.DeviceFamily = Scope->ActiveDeviceFamily;
    Event.BindingGeneration = BindingGeneration;
    Event.Sequence = ++Scope->Sequence;
    Publish(MoveTemp(Event));
}

void UGamePlatformInputLocalPlayerSubsystem::Interrupt(uint8 Channels, EGamePlatformInputEndReason Reason)
{
    if (!Scope || Channels == 0) { return; }

    for (int32 SemanticValue = static_cast<int32>(EGamePlatformInputSemantic::Move);
         SemanticValue <= static_cast<int32>(EGamePlatformInputSemantic::Cancel);
         ++SemanticValue)
    {
        const EGamePlatformInputSemantic Semantic = static_cast<EGamePlatformInputSemantic>(SemanticValue);
        if ((GamePlatformInputServices::GetChannel(Semantic) & Channels) == 0) { continue; }

        Policy::ActionGate& Gate = Scope->ActionGates[static_cast<std::size_t>(SemanticValue)];
        if (!Gate.Interrupt()) { continue; }

        FGamePlatformInputEvent Event;
        Event.Semantic = Semantic;
        Event.Phase = ETriggerEvent::Canceled;
        Event.Value = MakeNeutralValue(ExpectedValueType(Semantic));
        Event.Unit = GamePlatformInputServices::GetUnit(Semantic);
        Event.EndReason = Reason;
        Event.DeviceFamily = Scope->ActiveDeviceFamily;
        Event.BindingGeneration = Scope->BindingGeneration;
        Event.Sequence = ++Scope->Sequence;
        Publish(MoveTemp(Event));
    }
}

void UGamePlatformInputLocalPlayerSubsystem::Publish(FGamePlatformInputEvent Event)
{
    if (!Scope) { return; }

    TGuardValue<bool> Guard(Scope->bDispatching, true);
    ++Scope->TotalInputEventsPublished;
    for (auto It = Scope->Subscriptions.CreateIterator(); It; ++It)
    {
        if (!It.Value().Owner.IsValid())
        {
            It.RemoveCurrent();
            continue;
        }
        It.Value().Callback(Event);
        ++Scope->TotalSubscriberCallbacks;
    }
}

void UGamePlatformInputLocalPlayerSubsystem::RebuildMappings()
{
    if (!Scope) { return; }
    if (UEnhancedInputLocalPlayerSubsystem* Enhanced =
        GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
    {
        Scope->bMappingsApplied = false;
        Enhanced->RequestRebuildControlMappings();
        ++Scope->TotalMappingRebuildRequests;
    }
}

void UGamePlatformInputLocalPlayerSubsystem::OnMappingsRebuilt()
{
    if (!Scope) { return; }
    Scope->bMappingsApplied = true;
    ++Scope->SettingsRevision;
}

void UGamePlatformInputLocalPlayerSubsystem::OnContextAdded(const UInputMappingContext*)
{
    if (Scope && !Scope->bAdjustingContexts)
    {
        Scope->bMappingsApplied = false;
    }
}

void UGamePlatformInputLocalPlayerSubsystem::OnContextRemoved(const UInputMappingContext* Context)
{
    if (!Scope || Scope->bAdjustingContexts || !Context) { return; }

    // 外部代码移除了本服务仍有租约的上下文时，不自动抢回所有权；中断Gameplay并暴露诊断。
    for (const auto& Pair : Scope->ContextLeases)
    {
        UInputMappingContext* Owned = Scope->Contexts.FindRef(Pair.Value.ContextName).Get();
        if (Owned == Context)
        {
            Scope->LastResult = FGamePlatformResult::Failure(
                TEXT("ExternalInputContextMutation"),
                TEXT("平台持有租约的输入上下文被外部代码移除。"));
            Interrupt(
                static_cast<uint8>(EGamePlatformInputChannel::Move) |
                static_cast<uint8>(EGamePlatformInputChannel::Look) |
                static_cast<uint8>(EGamePlatformInputChannel::Actions),
                EGamePlatformInputEndReason::ContextRemoved);
            break;
        }
    }
    Scope->bMappingsApplied = false;
}
