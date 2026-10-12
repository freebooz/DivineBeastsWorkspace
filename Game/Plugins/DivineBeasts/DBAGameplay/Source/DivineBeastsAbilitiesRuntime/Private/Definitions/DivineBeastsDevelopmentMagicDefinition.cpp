// 项目开发魔法静态验证；父类身份、英雄与数值表的所有权保持不变，视觉由第三层内容包拥有。
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"
#include "Development/DivineBeastsDevelopmentAbilityPolicy.h"
bool UDivineBeastsDevelopmentMagicDefinition::IsDevelopmentEnabledForCurrentProcess()
{
    return DivineBeasts::DevelopmentAbilities::IsEnabledForCurrentProcess();
}
FGamePlatformResult UDivineBeastsDevelopmentMagicDefinition::ValidateDefinition() const
{
    const auto Parent = Super::ValidateDefinition();
    if (!Parent.IsSuccess()) return Parent;
    if (GetPresentationStyleId().IsNone() || !ProjectileMotion.IsValid() || ProjectileCount < 1 || ProjectileCount > 3 ||
        !FMath::IsFinite(ProjectileIntervalSeconds) ||
        (ProjectileCount == 1 ? ProjectileIntervalSeconds != 0 : ProjectileIntervalSeconds < 0.05f || ProjectileIntervalSeconds > 1.0f))
        return FGamePlatformResult::Failure(TEXT("DevelopmentMagicInvalid"), TEXT("开发魔法类型、运动参数或有界连发间隔不合法。"));
    return FGamePlatformResult::Success();
}
FName UDivineBeastsDevelopmentMagicDefinition::GetPresentationStyleId() const
{
    switch (MagicKind)
    {
    case EDivineBeastsDevelopmentMagicKind::Fireball: return TEXT("DBA.Development.Fireball");
    case EDivineBeastsDevelopmentMagicKind::Frostbolt: return TEXT("DBA.Development.Frostbolt");
    case EDivineBeastsDevelopmentMagicKind::ArcaneMissiles: return TEXT("DBA.Development.ArcaneMissiles");
    default: return NAME_None;
    }
}
