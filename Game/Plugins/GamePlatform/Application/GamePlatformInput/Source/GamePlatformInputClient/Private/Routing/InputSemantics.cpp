// 平台客户端输入标签与数值合同：供本地玩家路由、Profile编译及测试调用。
// 标签由平台配置拥有；本文件不持有玩家/世界状态，不提供游戏项目专属玩法。
// UObject依赖必须延迟到游戏线程的业务调用，避免单体Client在CRT初始化阶段崩溃。
#include "Services/GamePlatformInputServices.h"

#include "Definitions/GamePlatformInputProfileDefinition.h"
#include "GameplayTagsManager.h"

/*
 * 平台内建语义只把通用导航/视角/UI/交互作为长期合同。
 * Attack/AbilitySlot/TargetLock标签仅为旧EGamePlatformInputSemantic兼容层保留；
 * 新项目不得继续把这些Legacy标签作为平台公共语义扩展入口。
 */
namespace
{
// 稳定标签的只读值缓存；构造只发生在引擎配置/UObject完成初始化后的首次调用。
struct FPlatformInputTagCache
{
    FGameplayTag InputMove = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Move"), true);
    FGameplayTag InputLookDelta = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.LookDelta"), true);
    FGameplayTag InputLookRate = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.LookRate"), true);
    FGameplayTag InputInteract = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Interact"), true);
    FGameplayTag InputMenu = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Menu"), true);
    FGameplayTag InputConfirm = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Confirm"), true);
    FGameplayTag InputCancel = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Cancel"), true);

    // 兼容旧Profile身份，不增加或改名项目玩法；新版项目使用自己的Descriptor标签。
    FGameplayTag InputLegacyAttack = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Attack.Primary"), true);
    FGameplayTag InputLegacyAbility1 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Ability.Slot1"), true);
    FGameplayTag InputLegacyAbility2 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Ability.Slot2"), true);
    FGameplayTag InputLegacyAbility3 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Ability.Slot3"), true);
    FGameplayTag InputLegacyAbility4 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Ability.Slot4"), true);
    FGameplayTag InputLegacyTarget = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Target.Lock"), true);
};

// 不在进程加载阶段访问UGameplayTagsManager；所有调用方遵循客户端游戏线程契约。
const FPlatformInputTagCache& GetInputTags()
{
    check(IsInGameThread());
    static const FPlatformInputTagCache Tags;
    return Tags;
}

constexpr uint8 KnownInputChannels =
    static_cast<uint8>(EGamePlatformInputChannel::Move) |
    static_cast<uint8>(EGamePlatformInputChannel::Look) |
    static_cast<uint8>(EGamePlatformInputChannel::Actions) |
    static_cast<uint8>(EGamePlatformInputChannel::UICommands) |
    static_cast<uint8>(EGamePlatformInputChannel::TextEntry);

bool HasSingleChannelBit(uint8 ChannelMask)
{
    return ChannelMask != 0 &&
        (ChannelMask & ~KnownInputChannels) == 0 &&
        (ChannelMask & (ChannelMask - 1)) == 0;
}
}

FGameplayTag GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic Semantic)
{
    const FPlatformInputTagCache& Tags = GetInputTags();
    switch (Semantic)
    {
    case EGamePlatformBuiltInInputSemantic::Move: return Tags.InputMove;
    case EGamePlatformBuiltInInputSemantic::LookDelta: return Tags.InputLookDelta;
    case EGamePlatformBuiltInInputSemantic::LookRate: return Tags.InputLookRate;
    case EGamePlatformBuiltInInputSemantic::Interact: return Tags.InputInteract;
    case EGamePlatformBuiltInInputSemantic::Menu: return Tags.InputMenu;
    case EGamePlatformBuiltInInputSemantic::Confirm: return Tags.InputConfirm;
    case EGamePlatformBuiltInInputSemantic::Cancel: return Tags.InputCancel;
    default: return {};
    }
}

FGamePlatformInputSemanticDescriptor GamePlatformInputServices::GetBuiltInSemanticDescriptor(
    EGamePlatformBuiltInInputSemantic Semantic)
{
    FGamePlatformInputSemanticDescriptor Result;
    Result.SemanticId.Tag = GetBuiltInSemanticTag(Semantic);
    switch (Semantic)
    {
    case EGamePlatformBuiltInInputSemantic::Move:
        Result.Unit = EGamePlatformInputUnit::NormalizedAxis;
        Result.ValueType = EInputActionValueType::Axis2D;
        Result.ChannelMask = static_cast<uint8>(EGamePlatformInputChannel::Move);
        Result.ValuePolicy = EGamePlatformInputValuePolicy::MoveAxis;
        break;
    case EGamePlatformBuiltInInputSemantic::LookDelta:
        Result.Unit = EGamePlatformInputUnit::DegreesDelta;
        Result.ValueType = EInputActionValueType::Axis2D;
        Result.ChannelMask = static_cast<uint8>(EGamePlatformInputChannel::Look);
        Result.ValuePolicy = EGamePlatformInputValuePolicy::LookDelta;
        break;
    case EGamePlatformBuiltInInputSemantic::LookRate:
        Result.Unit = EGamePlatformInputUnit::DegreesPerSecond;
        Result.ValueType = EInputActionValueType::Axis2D;
        Result.ChannelMask = static_cast<uint8>(EGamePlatformInputChannel::Look);
        Result.ValuePolicy = EGamePlatformInputValuePolicy::LookRate;
        break;
    case EGamePlatformBuiltInInputSemantic::Menu:
    case EGamePlatformBuiltInInputSemantic::Confirm:
    case EGamePlatformBuiltInInputSemantic::Cancel:
        Result.Unit = EGamePlatformInputUnit::Boolean;
        Result.ValueType = EInputActionValueType::Boolean;
        Result.ChannelMask = static_cast<uint8>(EGamePlatformInputChannel::UICommands);
        Result.ValuePolicy = EGamePlatformInputValuePolicy::Passthrough;
        break;
    case EGamePlatformBuiltInInputSemantic::Interact:
        Result.Unit = EGamePlatformInputUnit::Boolean;
        Result.ValueType = EInputActionValueType::Boolean;
        Result.ChannelMask = static_cast<uint8>(EGamePlatformInputChannel::Actions);
        Result.ValuePolicy = EGamePlatformInputValuePolicy::Passthrough;
        break;
    default:
        break;
    }
    return Result;
}

FGameplayTag GamePlatformInputServices::GetSemanticTag(EGamePlatformInputSemantic Semantic)
{
    const FPlatformInputTagCache& Tags = GetInputTags();
    switch (Semantic)
    {
    case EGamePlatformInputSemantic::Move: return Tags.InputMove;
    case EGamePlatformInputSemantic::LookDelta: return Tags.InputLookDelta;
    case EGamePlatformInputSemantic::LookRate: return Tags.InputLookRate;
    case EGamePlatformInputSemantic::AttackPrimary: return Tags.InputLegacyAttack;
    case EGamePlatformInputSemantic::AbilitySlot1: return Tags.InputLegacyAbility1;
    case EGamePlatformInputSemantic::AbilitySlot2: return Tags.InputLegacyAbility2;
    case EGamePlatformInputSemantic::AbilitySlot3: return Tags.InputLegacyAbility3;
    case EGamePlatformInputSemantic::AbilitySlot4: return Tags.InputLegacyAbility4;
    case EGamePlatformInputSemantic::Interact: return Tags.InputInteract;
    case EGamePlatformInputSemantic::TargetLock: return Tags.InputLegacyTarget;
    case EGamePlatformInputSemantic::Menu: return Tags.InputMenu;
    case EGamePlatformInputSemantic::Confirm: return Tags.InputConfirm;
    case EGamePlatformInputSemantic::Cancel: return Tags.InputCancel;
    default: return {};
    }
}

EGamePlatformInputUnit GamePlatformInputServices::GetUnit(EGamePlatformInputSemantic Semantic)
{
    if (Semantic == EGamePlatformInputSemantic::Move) { return EGamePlatformInputUnit::NormalizedAxis; }
    if (Semantic == EGamePlatformInputSemantic::LookDelta) { return EGamePlatformInputUnit::DegreesDelta; }
    if (Semantic == EGamePlatformInputSemantic::LookRate) { return EGamePlatformInputUnit::DegreesPerSecond; }
    return EGamePlatformInputUnit::Boolean;
}

uint8 GamePlatformInputServices::GetChannel(EGamePlatformInputSemantic Semantic)
{
    switch (Semantic)
    {
    case EGamePlatformInputSemantic::Move:
        return static_cast<uint8>(EGamePlatformInputChannel::Move);
    case EGamePlatformInputSemantic::LookDelta:
    case EGamePlatformInputSemantic::LookRate:
        return static_cast<uint8>(EGamePlatformInputChannel::Look);
    case EGamePlatformInputSemantic::Menu:
    case EGamePlatformInputSemantic::Confirm:
    case EGamePlatformInputSemantic::Cancel:
        return static_cast<uint8>(EGamePlatformInputChannel::UICommands);
    case EGamePlatformInputSemantic::Interact:
        return static_cast<uint8>(EGamePlatformInputChannel::Actions);
    default:
        // 旧攻击/技能/锁定枚举只作为兼容动作继续走Gameplay Action通道。
        return static_cast<uint8>(EGamePlatformInputChannel::Actions);
    }
}

FGamePlatformInputSemanticDescriptor GamePlatformInputServices::GetLegacySemanticDescriptor(
    EGamePlatformInputSemantic Semantic)
{
    FGamePlatformInputSemanticDescriptor Result;
    switch (Semantic)
    {
    case EGamePlatformInputSemantic::Move:
        return GetBuiltInSemanticDescriptor(EGamePlatformBuiltInInputSemantic::Move);
    case EGamePlatformInputSemantic::LookDelta:
        return GetBuiltInSemanticDescriptor(EGamePlatformBuiltInInputSemantic::LookDelta);
    case EGamePlatformInputSemantic::LookRate:
        return GetBuiltInSemanticDescriptor(EGamePlatformBuiltInInputSemantic::LookRate);
    case EGamePlatformInputSemantic::Interact:
        return GetBuiltInSemanticDescriptor(EGamePlatformBuiltInInputSemantic::Interact);
    case EGamePlatformInputSemantic::Menu:
        return GetBuiltInSemanticDescriptor(EGamePlatformBuiltInInputSemantic::Menu);
    case EGamePlatformInputSemantic::Confirm:
        return GetBuiltInSemanticDescriptor(EGamePlatformBuiltInInputSemantic::Confirm);
    case EGamePlatformInputSemantic::Cancel:
        return GetBuiltInSemanticDescriptor(EGamePlatformBuiltInInputSemantic::Cancel);
    default:
        Result.SemanticId.Tag = GetSemanticTag(Semantic);
        Result.Unit = GetUnit(Semantic);
        Result.ValueType = EInputActionValueType::Boolean;
        Result.ChannelMask = GetChannel(Semantic);
        Result.ValuePolicy = EGamePlatformInputValuePolicy::Passthrough;
        return Result;
    }
}

bool GamePlatformInputServices::IsValidSemanticDescriptor(
    const FGamePlatformInputSemanticDescriptor& Descriptor)
{
    if (!Descriptor.SemanticId.IsValid() || !HasSingleChannelBit(Descriptor.ChannelMask))
    {
        return false;
    }

    // 平台只验证数值合同，不限制项目Tag命名空间；上层项目可以使用自己的Project.Input.*命名空间。
    switch (Descriptor.ValuePolicy)
    {
    case EGamePlatformInputValuePolicy::MoveAxis:
        return Descriptor.Unit == EGamePlatformInputUnit::NormalizedAxis &&
            Descriptor.ValueType == EInputActionValueType::Axis2D &&
            Descriptor.ChannelMask == static_cast<uint8>(EGamePlatformInputChannel::Move);
    case EGamePlatformInputValuePolicy::LookDelta:
        return Descriptor.Unit == EGamePlatformInputUnit::DegreesDelta &&
            Descriptor.ValueType == EInputActionValueType::Axis2D &&
            Descriptor.ChannelMask == static_cast<uint8>(EGamePlatformInputChannel::Look);
    case EGamePlatformInputValuePolicy::LookRate:
        return Descriptor.Unit == EGamePlatformInputUnit::DegreesPerSecond &&
            Descriptor.ValueType == EInputActionValueType::Axis2D &&
            Descriptor.ChannelMask == static_cast<uint8>(EGamePlatformInputChannel::Look);
    case EGamePlatformInputValuePolicy::Passthrough:
        if (Descriptor.Unit == EGamePlatformInputUnit::Boolean)
        {
            return Descriptor.ValueType == EInputActionValueType::Boolean;
        }
        if (Descriptor.Unit == EGamePlatformInputUnit::DegreesDelta ||
            Descriptor.Unit == EGamePlatformInputUnit::DegreesPerSecond)
        {
            return Descriptor.ValueType == EInputActionValueType::Axis2D;
        }
        return Descriptor.Unit == EGamePlatformInputUnit::NormalizedAxis;
    default:
        return false;
    }
}

bool GamePlatformInputServices::ResolveActionDescriptor(
    const FGamePlatformInputActionDefinition& Action,
    FGamePlatformInputSemanticDescriptor& OutDescriptor,
    bool& bOutHasLegacySemantic,
    EGamePlatformInputSemantic& OutLegacySemantic)
{
    if (Action.Descriptor.SemanticId.IsValid())
    {
        OutDescriptor = Action.Descriptor;
        bOutHasLegacySemantic = false;
        OutLegacySemantic = EGamePlatformInputSemantic::Move;
        return IsValidSemanticDescriptor(OutDescriptor);
    }

    OutLegacySemantic = Action.Semantic;
    OutDescriptor = GetLegacySemanticDescriptor(Action.Semantic);
    bOutHasLegacySemantic = true;

    // 旧资产继续要求Unit与历史语义一致，防止升级后悄悄改变输入物理单位。
    return Action.Unit == OutDescriptor.Unit && IsValidSemanticDescriptor(OutDescriptor);
}
