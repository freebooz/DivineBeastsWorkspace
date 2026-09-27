#include "Definitions/GamePlatformHeroDefinition.h"

bool FGamePlatformCharacterSpawnEnvelope::IsValid(FString& OutError) const
{
    if (!FMath::IsFinite(CapsuleRadius) ||
        !FMath::IsFinite(CapsuleHalfHeight) ||
        CapsuleRadius <= 0.0f ||
        CapsuleHalfHeight <= 0.0f)
    {
        OutError = TEXT("SpawnEnvelope胶囊尺寸必须为有限正数。");
        return false;
    }
    if (CapsuleHalfHeight < CapsuleRadius)
    {
        OutError = TEXT("CapsuleHalfHeight必须大于或等于CapsuleRadius。");
        return false;
    }
    if (CollisionProfileName.IsNone())
    {
        OutError = TEXT("CollisionProfileName不能为空。");
        return false;
    }
    return true;
}

bool FGamePlatformCharacterMovementDefinition::IsValid(FString& OutError) const
{
    if (!FMath::IsFinite(MaxWalkSpeed) ||
        !FMath::IsFinite(MaxAcceleration) ||
        !FMath::IsFinite(JumpZVelocity) ||
        !FMath::IsFinite(RotationRateYaw))
    {
        OutError = TEXT("Movement参数必须为有限数值。");
        return false;
    }
    if (MaxWalkSpeed <= 0.0f || MaxAcceleration <= 0.0f ||
        JumpZVelocity < 0.0f || RotationRateYaw < 0.0f)
    {
        OutError = TEXT("Movement参数超出允许范围。");
        return false;
    }
    return true;
}

FPrimaryAssetId UGamePlatformHeroDefinition::GetPrimaryAssetId() const
{
    return DefinitionId.IsNone()
        ? FPrimaryAssetId()
        : FPrimaryAssetId(FPrimaryAssetType(TEXT("GamePlatformHeroDefinition")), DefinitionId);
}

bool UGamePlatformHeroDefinition::IsDefinitionValid(FString& OutError) const
{
    if (DefinitionId.IsNone())
    {
        OutError = TEXT("DefinitionId不能为空。");
        return false;
    }
    if (Version <= 0)
    {
        OutError = TEXT("Version必须大于0。");
        return false;
    }
    if (ContentRevision.TrimStartAndEnd().IsEmpty())
    {
        OutError = TEXT("ContentRevision不能为空。");
        return false;
    }
    if (!SpawnEnvelope.IsValid(OutError))
    {
        return false;
    }
    if (!Movement.IsValid(OutError))
    {
        return false;
    }

    if (RequiredDefinitions.Num() > 32)
    {
        OutError = TEXT("RequiredDefinitions数量不能超过32，过大的依赖扇出应拆分到更高层组合Definition。");
        return false;
    }

    TSet<FName> SeenDependencies;
    for (const FName RequiredDefinition : RequiredDefinitions)
    {
        if (RequiredDefinition.IsNone() ||
            RequiredDefinition == DefinitionId ||
            SeenDependencies.Contains(RequiredDefinition))
        {
            OutError = TEXT("RequiredDefinitions不能包含空值、自引用或重复项。");
            return false;
        }
        SeenDependencies.Add(RequiredDefinition);
    }
    return true;
}
