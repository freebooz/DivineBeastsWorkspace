#pragma once

#include "Contracts/Domains/DivineBeastsLiveOpsUIContracts.h"
#include "ViewModels/Domains/DivineBeastsScopedDomainViewModelBase.h"
#include "DivineBeastsLiveOpsViewModel.generated.h"

/** 运营界面只读视图模型：数据由外部授权服务适配器提供，不在UI计算活动奖励。 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsLiveOpsViewModel
    : public UDivineBeastsScopedDomainViewModelBase
{
    GENERATED_BODY()
public:
    /** 仅C++来源适配器写入；拒绝迟到修订、未绑定账号、非法计数。 */
    bool ApplyLiveOpsSnapshot(const FDivineBeastsLiveOpsUIProjection& Next);
    /** 蓝图读取，服务未接入时为显式不可用的空快照。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|LiveOps")
    FDivineBeastsLiveOpsUIProjection GetLiveOpsSnapshot() const { return Snapshot; }
    const FDivineBeastsLiveOpsUIProjection& GetSnapshotRef() const { return Snapshot; }
protected:
    virtual void ResetDomainProjection() override { Snapshot = FDivineBeastsLiveOpsUIProjection(); }
private:
    UPROPERTY(Transient) FDivineBeastsLiveOpsUIProjection Snapshot;
};
