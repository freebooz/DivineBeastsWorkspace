#include "Definitions/GamePlatformInputProfileDefinition.h"
#include "Services/GamePlatformInputServices.h"

FGamePlatformResult UGamePlatformInputProfileDefinition::ValidateDefinition() const
{
    const auto Base = Super::ValidateDefinition(); if (!Base.IsSuccess()) { return Base; }
    if (Actions.Num() < 3 || Actions.Num() > 32 || Contexts.IsEmpty() || Contexts.Num() > 8 ||
        !FMath::IsFinite(LookDegreesPerCount) || LookDegreesPerCount < 0.001 || LookDegreesPerCount > 10 ||
        !FMath::IsFinite(LookDegreesPerSecond) || LookDegreesPerSecond < 1 || LookDegreesPerSecond > 720 ||
        !FMath::IsFinite(AnalogDeadZone) || AnalogDeadZone < 0 || AnalogDeadZone > 0.5)
    { return FGamePlatformResult::Failure(TEXT("InvalidInputProfile"),TEXT("动作/映射数量或灵敏度/死区超出有限支持范围")); }
    TSet<EGamePlatformInputSemantic> Semantics; TSet<FSoftObjectPath> Assets; TSet<FName> Names;
    for (const auto& Entry : Actions)
    {
        if (static_cast<uint8>(Entry.Semantic) > static_cast<uint8>(EGamePlatformInputSemantic::Cancel) || Entry.Action.IsNull() ||
            Semantics.Contains(Entry.Semantic) || Assets.Contains(Entry.Action.ToSoftObjectPath()) || Entry.Unit != GamePlatformInputServices::GetUnit(Entry.Semantic))
        { return FGamePlatformResult::Failure(TEXT("InvalidInputAction"),TEXT("重复语义/动作、缺少资产或单位不匹配")); }
        Semantics.Add(Entry.Semantic); Assets.Add(Entry.Action.ToSoftObjectPath());
    }
    if (!Semantics.Contains(EGamePlatformInputSemantic::Move) || !Semantics.Contains(EGamePlatformInputSemantic::Menu) || !Semantics.Contains(EGamePlatformInputSemantic::Cancel))
    { return FGamePlatformResult::Failure(TEXT("RequiredInputMissing"),TEXT("至少需要移动、菜单和取消语义")); }
    Assets.Reset();
    for (const auto& Entry : Contexts)
    {
        if (Entry.Name.IsNone() || Entry.Context.IsNull() || Names.Contains(Entry.Name) || Assets.Contains(Entry.Context.ToSoftObjectPath()))
        { return FGamePlatformResult::Failure(TEXT("InvalidInputContext"),TEXT("映射名字/资产必须存在且唯一")); }
        Names.Add(Entry.Name); Assets.Add(Entry.Context.ToSoftObjectPath());
    }
    return FGamePlatformResult::Success();
}
