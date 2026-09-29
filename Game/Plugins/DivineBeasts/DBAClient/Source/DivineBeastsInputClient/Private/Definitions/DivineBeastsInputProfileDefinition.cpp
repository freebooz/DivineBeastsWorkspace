#include "Definitions/DivineBeastsInputProfileDefinition.h"

#include "Input/DivineBeastsInputSemantics.h"
#include "Services/GamePlatformInputServices.h"

FGamePlatformResult UDivineBeastsInputProfileDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess() || !bRequireCoreGameplaySemantics)
    {
        return Base;
    }

    TSet<FGameplayTag> ProjectSemantics;
    for (const FGamePlatformInputActionDefinition& Entry : Actions)
    {
        FGamePlatformInputSemanticDescriptor Descriptor;
        bool bHasLegacy = false;
        EGamePlatformInputSemantic LegacySemantic = EGamePlatformInputSemantic::Move;
        if (!GamePlatformInputServices::ResolveActionDescriptor(
                Entry,
                Descriptor,
                bHasLegacy,
                LegacySemantic))
        {
            return FGamePlatformResult::Failure(
                TEXT("DivineBeastsInputDescriptorInvalid"),
                TEXT("神兽联盟输入Profile包含无法解析的语义描述。"));
        }

        if (bHasLegacy &&
            (LegacySemantic == EGamePlatformInputSemantic::AttackPrimary ||
             LegacySemantic == EGamePlatformInputSemantic::AbilitySlot1 ||
             LegacySemantic == EGamePlatformInputSemantic::AbilitySlot2 ||
             LegacySemantic == EGamePlatformInputSemantic::AbilitySlot3 ||
             LegacySemantic == EGamePlatformInputSemantic::AbilitySlot4 ||
             LegacySemantic == EGamePlatformInputSemantic::TargetLock))
        {
            return FGamePlatformResult::Failure(
                TEXT("DivineBeastsLegacyGameplayInputForbidden"),
                TEXT("神兽联盟Gameplay Profile必须使用DivineBeasts.Input.*项目语义，禁止继续使用平台旧攻击/技能/锁定枚举。"));
        }

        if (DivineBeastsInputSemantics::IsCoreGameplaySemantic(Descriptor.SemanticId.Tag))
        {
            // 项目核心战斗语义必须是Boolean/Passthrough/Actions，禁止数据资产改变其运行物理单位。
            const FGamePlatformInputSemanticDescriptor Expected =
                DivineBeastsInputSemantics::MakeGameplayActionDescriptor(Descriptor.SemanticId.Tag);
            if (Descriptor.Unit != Expected.Unit ||
                Descriptor.ValueType != Expected.ValueType ||
                Descriptor.ChannelMask != Expected.ChannelMask ||
                Descriptor.ValuePolicy != Expected.ValuePolicy)
            {
                return FGamePlatformResult::Failure(
                    TEXT("DivineBeastsInputContractMismatch"),
                    TEXT("神兽联盟核心战斗语义的单位、值类型、通道或值处理策略被错误修改。"));
            }
            ProjectSemantics.Add(Descriptor.SemanticId.Tag);
        }
    }

    const FGameplayTag Required[] =
    {
        DivineBeastsInputSemantics::AttackPrimary(),
        DivineBeastsInputSemantics::AbilitySlot1(),
        DivineBeastsInputSemantics::AbilitySlot2(),
        DivineBeastsInputSemantics::AbilitySlot3(),
        DivineBeastsInputSemantics::AbilitySlot4(),
        DivineBeastsInputSemantics::TargetLock()
    };
    for (const FGameplayTag Tag : Required)
    {
        if (!ProjectSemantics.Contains(Tag))
        {
            return FGamePlatformResult::Failure(
                TEXT("DivineBeastsRequiredInputMissing"),
                TEXT("神兽联盟Gameplay输入Profile缺少核心攻击、技能槽或目标锁定语义。"));
        }
    }

    return FGamePlatformResult::Success();
}
