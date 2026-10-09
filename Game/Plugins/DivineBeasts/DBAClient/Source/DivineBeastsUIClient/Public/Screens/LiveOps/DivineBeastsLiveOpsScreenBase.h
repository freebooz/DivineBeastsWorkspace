#pragma once

#include "Contracts/Domains/DivineBeastsLiveOpsUIContracts.h"
#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsLiveOpsScreenBase.generated.h"

class UDivineBeastsLiveOpsViewModel;

/** 项目公告/活动/邮件的共同页面基类；商城支付流程继续归CommerceUI所有。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsLiveOpsScreenBase : public UDivineBeastsUIScreen
{
    GENERATED_BODY()
public:
    UDivineBeastsLiveOpsScreenBase() { UIDomain = EDivineBeastsUIDomain::LiveOps; }
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|LiveOps")
    UDivineBeastsLiveOpsViewModel* GetLiveOpsViewModel() const;
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|LiveOps")
    FDivineBeastsLiveOpsUIProjection GetLiveOpsProjection() const;
};
