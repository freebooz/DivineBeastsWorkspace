#include "Services/GamePlatformInputServices.h"

#include "Definitions/GamePlatformInputProfileDefinition.h"
#include "NativeGameplayTags.h"

/*
 * 平台内建语义只把通用导航/视角/UI/交互作为长期合同。
 * Attack/AbilitySlot/TargetLock标签仅为旧EGamePlatformInputSemantic兼容层保留；
 * 新项目不得继续把这些Legacy标签作为平台公共语义扩展入口。
 */
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputMove,"Platform.Input.Move");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputLookDelta,"Platform.Input.LookDelta");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputLookRate,"Platform.Input.LookRate");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputInteract,"Platform.Input.Interact");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputMenu,"Platform.Input.Menu");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputConfirm,"Platform.Input.Confirm");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputCancel,"Platform.Input.Cancel");

// 旧标签字符串必须保持不变，避免已有Profile/调用方因架构迁移发生静默兼容破坏；仅“归属职责”降级为Legacy。
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputLegacyAttack,"Platform.Input.Attack.Primary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputLegacyAbility1,"Platform.Input.Ability.Slot1");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputLegacyAbility2,"Platform.Input.Ability.Slot2");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputLegacyAbility3,"Platform.Input.Ability.Slot3");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputLegacyAbility4,"Platform.Input.Ability.Slot4");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputLegacyTarget,"Platform.Input.Target.Lock");

namespace
{
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
    switch (Semantic)
    {
    case EGamePlatformBuiltInInputSemantic::Move: return InputMove;
    case EGamePlatformBuiltInInputSemantic::LookDelta: return InputLookDelta;
    case EGamePlatformBuiltInInputSemantic::LookRate: return InputLookRate;
    case EGamePlatformBuiltInInputSemantic::Interact: return InputInteract;
    case EGamePlatformBuiltInInputSemantic::Menu: return InputMenu;
    case EGamePlatformBuiltInInputSemantic::Confirm: return InputConfirm;
    case EGamePlatformBuiltInInputSemantic::Cancel: return InputCancel;
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
    switch (Semantic)
    {
    case EGamePlatformInputSemantic::Move: return InputMove;
    case EGamePlatformInputSemantic::LookDelta: return InputLookDelta;
    case EGamePlatformInputSemantic::LookRate: return InputLookRate;
    case EGamePlatformInputSemantic::AttackPrimary: return InputLegacyAttack;
    case EGamePlatformInputSemantic::AbilitySlot1: return InputLegacyAbility1;
    case EGamePlatformInputSemantic::AbilitySlot2: return InputLegacyAbility2;
    case EGamePlatformInputSemantic::AbilitySlot3: return InputLegacyAbility3;
    case EGamePlatformInputSemantic::AbilitySlot4: return InputLegacyAbility4;
    case EGamePlatformInputSemantic::Interact: return InputInteract;
    case EGamePlatformInputSemantic::TargetLock: return InputLegacyTarget;
    case EGamePlatformInputSemantic::Menu: return InputMenu;
    case EGamePlatformInputSemantic::Confirm: return InputConfirm;
    case EGamePlatformInputSemantic::Cancel: return InputCancel;
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
