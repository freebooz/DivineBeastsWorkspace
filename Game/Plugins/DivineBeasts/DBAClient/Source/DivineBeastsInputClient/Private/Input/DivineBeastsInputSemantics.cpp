// 神兽联盟客户端输入语义：供本地玩家输入适配与合同测试读取项目标签。
// 标签身份由DefaultGameplayTags.ini拥有；这里只缓存只读值，不注册或改变平台输入规则。
// 单体Client在CRT静态初始化时尚无UObject系统，必须等游戏线程首次调用后再请求标签。
#include "Input/DivineBeastsInputSemantics.h"

#include "GameplayTagsManager.h"

namespace
{
    // 值缓存只在首次业务调用时构造；配置缺失仍由RequestGameplayTag报告，不生成替代身份。
    struct FDivineBeastsInputTagCache
    {
        FGameplayTag AttackPrimary = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("DivineBeasts.Input.Combat.Primary"), true);
        FGameplayTag AbilitySlot1 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("DivineBeasts.Input.Ability.Slot1"), true);
        FGameplayTag AbilitySlot2 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("DivineBeasts.Input.Ability.Slot2"), true);
        FGameplayTag AbilitySlot3 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("DivineBeasts.Input.Ability.Slot3"), true);
        FGameplayTag AbilitySlot4 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("DivineBeasts.Input.Ability.Slot4"), true);
        FGameplayTag TargetLock = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("DivineBeasts.Input.Target.Lock"), true);

        // GAS技能输入必须位于平台规定根下；项目只持有DivineBeasts子命名空间。
        FGameplayTag AbilityInputPrimary = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Ability.Input.DivineBeasts.Primary"), true);
        FGameplayTag AbilityInputSlot1 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Ability.Input.DivineBeasts.Slot1"), true);
        FGameplayTag AbilityInputSlot2 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Ability.Input.DivineBeasts.Slot2"), true);
        FGameplayTag AbilityInputSlot3 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Ability.Input.DivineBeasts.Slot3"), true);
        FGameplayTag AbilityInputSlot4 = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Ability.Input.DivineBeasts.Slot4"), true);
    };

    // UObject标签管理器只能在引擎已初始化的游戏线程使用；缓存不保存玩家或世界状态。
    const FDivineBeastsInputTagCache& GetInputTags()
    {
        check(IsInGameThread());
        static const FDivineBeastsInputTagCache Tags;
        return Tags;
    }
}

FGameplayTag DivineBeastsInputSemantics::AttackPrimary() { return GetInputTags().AttackPrimary; }
FGameplayTag DivineBeastsInputSemantics::AbilitySlot1() { return GetInputTags().AbilitySlot1; }
FGameplayTag DivineBeastsInputSemantics::AbilitySlot2() { return GetInputTags().AbilitySlot2; }
FGameplayTag DivineBeastsInputSemantics::AbilitySlot3() { return GetInputTags().AbilitySlot3; }
FGameplayTag DivineBeastsInputSemantics::AbilitySlot4() { return GetInputTags().AbilitySlot4; }
FGameplayTag DivineBeastsInputSemantics::TargetLock() { return GetInputTags().TargetLock; }

FGameplayTag DivineBeastsInputSemantics::ToAbilityInputTag(FGameplayTag SemanticTag)
{
    const FDivineBeastsInputTagCache& Tags = GetInputTags();
    if (SemanticTag == Tags.AttackPrimary) { return Tags.AbilityInputPrimary; }
    if (SemanticTag == Tags.AbilitySlot1) { return Tags.AbilityInputSlot1; }
    if (SemanticTag == Tags.AbilitySlot2) { return Tags.AbilityInputSlot2; }
    if (SemanticTag == Tags.AbilitySlot3) { return Tags.AbilityInputSlot3; }
    if (SemanticTag == Tags.AbilitySlot4) { return Tags.AbilityInputSlot4; }
    // TargetLock属于目标/角色控制语义，不伪装成GAS技能输入。
    return FGameplayTag();
}

FGamePlatformInputSemanticDescriptor DivineBeastsInputSemantics::MakeGameplayActionDescriptor(
    FGameplayTag SemanticTag)
{
    FGamePlatformInputSemanticDescriptor Result;
    Result.SemanticId.Tag = SemanticTag;
    Result.Unit = EGamePlatformInputUnit::Boolean;
    Result.ValueType = EInputActionValueType::Boolean;
    Result.ChannelMask = static_cast<uint8>(EGamePlatformInputChannel::Actions);
    Result.ValuePolicy = EGamePlatformInputValuePolicy::Passthrough;
    return Result;
}

bool DivineBeastsInputSemantics::IsCoreGameplaySemantic(FGameplayTag SemanticTag)
{
    const FDivineBeastsInputTagCache& Tags = GetInputTags();
    return SemanticTag == Tags.AttackPrimary ||
        SemanticTag == Tags.AbilitySlot1 ||
        SemanticTag == Tags.AbilitySlot2 ||
        SemanticTag == Tags.AbilitySlot3 ||
        SemanticTag == Tags.AbilitySlot4 ||
        SemanticTag == Tags.TargetLock;
}
