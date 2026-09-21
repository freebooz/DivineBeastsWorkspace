#include "Definitions/GamePlatformRegionDefinition.h"

FGamePlatformResult UGamePlatformRegionDefinition::ValidateDefinition() const
{
    check(IsInGameThread());
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess()) { return BaseResult; }
    if (RegionTypeTag.IsNone() || RegionTypeTag.ToString().TrimStartAndEnd().IsEmpty())
    { return FGamePlatformResult::Failure(TEXT("RegionTypeMissing"), TEXT("区域定义必须声明中立区域语义名。")); }
    const bool bEmptyParent = ParentRegionId.Namespace.IsEmpty() && ParentRegionId.Name.IsEmpty() && ParentRegionId.LogicalVersion == 1;
    if (!bEmptyParent)
    {
        if (!ParentRegionId.IsValid())
        { return FGamePlatformResult::Failure(TEXT("InvalidParentRegionId"), TEXT("父区域必须留为完整默认空值，或填写合法逻辑身份。")); }
        if (ParentRegionId == LogicalId)
        { return FGamePlatformResult::Failure(TEXT("RegionParentSelfReference"), TEXT("区域不能以自身为父区域。")); }
    }
    if (BoundsPolicy != EGamePlatformRegionBoundsPolicy::AxisAlignedBox)
    { return FGamePlatformResult::Unsupported(TEXT("UnsupportedRegionBoundsPolicy"), TEXT("当前只实现由Provider提供的轴对齐盒边界。")); }
    if (ActivationPolicy != EGamePlatformRegionActivationPolicy::AlwaysRegistered)
    { return FGamePlatformResult::Unsupported(TEXT("UnsupportedRegionActivationPolicy"), TEXT("当前只实现Provider有效注册期间参与查询的激活策略。")); }
    return FGamePlatformResult::Success();
}
