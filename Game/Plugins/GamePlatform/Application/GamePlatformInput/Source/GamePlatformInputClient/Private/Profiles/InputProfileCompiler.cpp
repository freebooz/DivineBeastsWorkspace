#include "Profiles/InputProfileCompiler.h"

#include "InputAction.h"
#include "Services/GamePlatformInputServices.h"

FGamePlatformResult CompileGamePlatformInputProfile(
    const UGamePlatformInputProfileDefinition& Profile,
    TArray<FGamePlatformCompiledInputAction>& OutCompiledActions,
    TMap<FGameplayTag, int32>& OutSlotByTag,
    TMap<uint8, int32>& OutLegacySlot)
{
    OutCompiledActions.Reset();
    OutSlotByTag.Reset();
    OutLegacySlot.Reset();

    // Profile动作数量已有Definition上限；这里仍预分配，避免准备阶段多次扩容。
    OutCompiledActions.Reserve(Profile.Actions.Num());
    OutSlotByTag.Reserve(Profile.Actions.Num());
    OutLegacySlot.Reserve(Profile.Actions.Num());

    for (const FGamePlatformInputActionDefinition& Entry : Profile.Actions)
    {
        FGamePlatformInputSemanticDescriptor Descriptor;
        bool bHasLegacySemantic = false;
        EGamePlatformInputSemantic LegacySemantic = EGamePlatformInputSemantic::Move;
        if (!GamePlatformInputServices::ResolveActionDescriptor(
                Entry,
                Descriptor,
                bHasLegacySemantic,
                LegacySemantic))
        {
            OutCompiledActions.Reset();
            OutSlotByTag.Reset();
            OutLegacySlot.Reset();
            return FGamePlatformResult::Failure(
                TEXT("InvalidInputSemanticDescriptor"),
                TEXT("输入动作语义描述无法解析为有效的平台运行时合同。"));
        }

        UInputAction* Action = Entry.Action.Get();
        if (!Action || Action->ValueType != Descriptor.ValueType)
        {
            OutCompiledActions.Reset();
            OutSlotByTag.Reset();
            OutLegacySlot.Reset();
            return FGamePlatformResult::Failure(
                TEXT("InvalidLoadedInputAction"),
                TEXT("Input Bundle未加载动作或动作值维度与编译后语义描述不匹配。"));
        }

        if (OutSlotByTag.Contains(Descriptor.SemanticId.Tag))
        {
            OutCompiledActions.Reset();
            OutSlotByTag.Reset();
            OutLegacySlot.Reset();
            return FGamePlatformResult::Failure(
                TEXT("DuplicateInputSemantic"),
                TEXT("同一稳定输入语义在Profile中只能声明一次。"));
        }

        FGamePlatformCompiledInputAction Compiled;
        Compiled.Slot = OutCompiledActions.Num();
        Compiled.SemanticId = Descriptor.SemanticId;
        Compiled.Unit = Descriptor.Unit;
        Compiled.ValueType = Descriptor.ValueType;
        Compiled.ChannelMask = Descriptor.ChannelMask;
        Compiled.ValuePolicy = Descriptor.ValuePolicy;
        Compiled.bHasLegacySemantic = bHasLegacySemantic;
        Compiled.LegacySemantic = LegacySemantic;
        Compiled.Action = Action;

        OutSlotByTag.Add(Descriptor.SemanticId.Tag, Compiled.Slot);
        if (bHasLegacySemantic)
        {
            OutLegacySlot.Add(static_cast<uint8>(LegacySemantic), Compiled.Slot);
        }
        OutCompiledActions.Add(MoveTemp(Compiled));
    }

    return FGamePlatformResult::Success();
}
