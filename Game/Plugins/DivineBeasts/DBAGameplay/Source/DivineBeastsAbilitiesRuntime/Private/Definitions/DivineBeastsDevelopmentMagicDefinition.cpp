// 项目开发魔法静态验证；父类身份、英雄与数值表的所有权保持不变，视觉由第三层内容包拥有。
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"
#include "Development/DivineBeastsDevelopmentAbilityPolicy.h"
UDivineBeastsDevelopmentMagicDefinition::UDivineBeastsDevelopmentMagicDefinition()
{
    ProjectileMotion.SpeedCmPerSecond = DefaultProjectileSpeedCmPerSecond;
}
float UDivineBeastsDevelopmentMagicDefinition::GetCastDurationSeconds(
    EDivineBeastsDevelopmentMagicKind Kind, FName AbilityId)
{
    // 定向飞弹保留既有权威时序；按可信数值行身份识别，不读取可选表现覆盖或客户端参数。
    if (Kind == EDivineBeastsDevelopmentMagicKind::Fireball)
        return AbilityId.ToString().EndsWith(TEXT("_placeholder_directedmissile@1")) ? 0.5f : 3.f;
    return Kind == EDivineBeastsDevelopmentMagicKind::Frostbolt ? 0.5f : 0.f;
}
float UDivineBeastsDevelopmentMagicDefinition::GetMonsterFreezeDurationSeconds(
    EDivineBeastsDevelopmentMagicKind Kind)
{
    return Kind == EDivineBeastsDevelopmentMagicKind::Frostbolt ? 3.f : 0.f;
}
bool UDivineBeastsDevelopmentMagicDefinition::IsProjectileSpeedSupported(float SpeedCmPerSecond)
{
    return FMath::IsFinite(SpeedCmPerSecond) && SpeedCmPerSecond >= MinProjectileSpeedCmPerSecond &&
        SpeedCmPerSecond <= MaxProjectileSpeedCmPerSecond;
}
bool UDivineBeastsDevelopmentMagicDefinition::IsDevelopmentEnabledForCurrentProcess()
{
    return DivineBeasts::DevelopmentAbilities::IsEnabledForCurrentProcess();
}
bool UDivineBeastsDevelopmentMagicDefinition::IsProjectileMotionSupported(const FGamePlatformCombatProjectileMotion& Motion)
{
    const auto& Config=Motion.ResolveFlightConfig();
    return Motion.IsValid() && (Config.bEnabled ? Config.InitialSpeed>0 && Config.MaxSpeed<=MaxProjectileSpeedCmPerSecond : IsProjectileSpeedSupported(Motion.SpeedCmPerSecond));
}
FGamePlatformResult UDivineBeastsDevelopmentMagicDefinition::ValidateDefinition() const
{
    const auto Parent = Super::ValidateDefinition();
    if (!Parent.IsSuccess()) return Parent;
    if (GetPresentationStyleId().IsNone() || !ProjectileMotion.IsValid() ||
        !IsProjectileMotionSupported(ProjectileMotion) || ProjectileCount < 1 || ProjectileCount > 3 ||
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
