#include "Definitions/GamePlatformInputProfileDefinition.h"
#include "Services/GamePlatformInputServices.h"

FGamePlatformResult UGamePlatformInputProfileDefinition::ValidateDefinition() const
{
    const auto Base = Super::ValidateDefinition(); if (!Base.IsSuccess()) { return Base; }
    if (Actions.Num() < 3 || Actions.Num() > 32 || Contexts.IsEmpty() || Contexts.Num() > 8 ||
        !FMath::IsFinite(LookDegreesPerCount) || LookDegreesPerCount < 0.001 || LookDegreesPerCount > 10 ||
        !FMath::IsFinite(LookDegreesPerSecond) || LookDegreesPerSecond < 1 || LookDegreesPerSecond > 720 ||
        !FMath::IsFinite(AnalogDeadZone) || AnalogDeadZone < 0 || AnalogDeadZone > 0.5 ||
        !FMath::IsFinite(TouchAnalogDeadZone) || TouchAnalogDeadZone < 0 || TouchAnalogDeadZone > 0.5 ||
        !FMath::IsFinite(DefaultAccessibility.LookSensitivityMultiplier) || DefaultAccessibility.LookSensitivityMultiplier < 0.1 || DefaultAccessibility.LookSensitivityMultiplier > 5.0 ||
        !FMath::IsFinite(DefaultAccessibility.MoveDeadZoneMultiplier) || DefaultAccessibility.MoveDeadZoneMultiplier < 0.5 || DefaultAccessibility.MoveDeadZoneMultiplier > 2.0 ||
        !FMath::IsFinite(DefaultAccessibility.TouchLookSensitivityMultiplier) || DefaultAccessibility.TouchLookSensitivityMultiplier < 0.25 || DefaultAccessibility.TouchLookSensitivityMultiplier > 3.0 ||
        !FMath::IsFinite(DefaultAccessibility.TouchMoveScale) || DefaultAccessibility.TouchMoveScale < 0.5 || DefaultAccessibility.TouchMoveScale > 1.5 ||
        (!bEnableKeyboardMouse && !bEnableGamepad && !bEnableTouch))
    { return FGamePlatformResult::Failure(TEXT("InvalidInputProfile"),TEXT("动作/映射数量、设备开关、灵敏度或死区超出有限支持范围")); }
    TSet<FGameplayTag> Semantics; TSet<FSoftObjectPath> Assets; TSet<FName> Names;
    for (const auto& Entry : Actions)
    {
        FGamePlatformInputSemanticDescriptor Descriptor;
        bool bHasLegacy = false;
        EGamePlatformInputSemantic LegacySemantic = EGamePlatformInputSemantic::Move;
        if (Entry.Action.IsNull() ||
            !GamePlatformInputServices::ResolveActionDescriptor(Entry, Descriptor, bHasLegacy, LegacySemantic) ||
            Semantics.Contains(Descriptor.SemanticId.Tag) || Assets.Contains(Entry.Action.ToSoftObjectPath()))
        {
            return FGamePlatformResult::Failure(
                TEXT("InvalidInputAction"),
                TEXT("输入动作必须具有唯一有效语义描述、唯一资产和匹配的单位/通道/值类型。"));
        }
        Semantics.Add(Descriptor.SemanticId.Tag);
        Assets.Add(Entry.Action.ToSoftObjectPath());
    }

    // 只要求真正跨游戏稳定的基础语义；攻击/技能/锁定由上层项目Profile自行声明。
    if (!Semantics.Contains(GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::Move)) ||
        !Semantics.Contains(GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::Menu)) ||
        !Semantics.Contains(GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::Cancel)))
    {
        return FGamePlatformResult::Failure(TEXT("RequiredInputMissing"),TEXT("至少需要移动、菜单和取消三个平台基础语义。"));
    }
    Assets.Reset();
    for (const auto& Entry : Contexts)
    {
        if (Entry.Name.IsNone() || Entry.Context.IsNull() || Names.Contains(Entry.Name) || Assets.Contains(Entry.Context.ToSoftObjectPath()))
        { return FGamePlatformResult::Failure(TEXT("InvalidInputContext"),TEXT("映射名字/资产必须存在且唯一")); }
        Names.Add(Entry.Name); Assets.Add(Entry.Context.ToSoftObjectPath());
    }
    return FGamePlatformResult::Success();
}
