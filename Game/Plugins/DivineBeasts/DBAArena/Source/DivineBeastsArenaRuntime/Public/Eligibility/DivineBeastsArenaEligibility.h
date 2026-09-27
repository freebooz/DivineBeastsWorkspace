#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

/**
 * IDivineBeastsArenaTrustedHeroEligibilityProvider（可信竞技英雄资格提供者）。
 * 由服务器组合根连接Entitlement/maintenance数据；客户端不可作为权威实现。
 */
class IDivineBeastsArenaTrustedHeroEligibilityProvider
    : public IModularFeature
{
public:
    virtual ~IDivineBeastsArenaTrustedHeroEligibilityProvider() = default;

    static FName GetModularFeatureName()
    {
        static const FName Name(TEXT("DivineBeasts.Arena.TrustedHeroEligibility"));
        return Name;
    }

    virtual bool IsHeroEligible(
        const FString& PlayerId,
        FName HeroDefinitionId,
        FName ArenaModeId,
        FString& OutReason) const = 0;
};
