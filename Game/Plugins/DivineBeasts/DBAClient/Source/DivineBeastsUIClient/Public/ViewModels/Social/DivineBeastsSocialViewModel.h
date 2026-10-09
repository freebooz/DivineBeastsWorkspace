#pragma once

#include "Contracts/Domains/DivineBeastsSocialUIContracts.h"
#include "ViewModels/Domains/DivineBeastsScopedDomainViewModelBase.h"
#include "DivineBeastsSocialViewModel.generated.h"

/** 社交UI只读视图模型：由授权社交适配器明确投送快照，不直接联网。 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsSocialViewModel
    : public UDivineBeastsScopedDomainViewModelBase
{
    GENERATED_BODY()
public:
    /** 仅C++适配器可提供快照；拒绝账号串号、回退版本和超量列表。 */
    bool ApplySocialSnapshot(const FDivineBeastsSocialUIProjection& Next);
    /** 蓝图值返回接口；列表规模上限在写入时检查。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Social")
    FDivineBeastsSocialUIProjection GetSocialSnapshot() const { return Snapshot; }
    /** C++低分配访问接口，不复制列表。 */
    const FDivineBeastsSocialUIProjection& GetSnapshotRef() const { return Snapshot; }
protected:
    virtual void ResetDomainProjection() override { Snapshot = FDivineBeastsSocialUIProjection(); }
private:
    UPROPERTY(Transient) FDivineBeastsSocialUIProjection Snapshot;
};
