#pragma once

#include "ViewModels/DivineBeastsViewModelBase.h"
#include "DivineBeastsScopedDomainViewModelBase.generated.h"

/**
 * UDivineBeastsScopedDomainViewModelBase（账号/来源作用域视图模型）。
 * 社交、邮件、活动等延迟回调必须同时验证来源代次和修订号，防止注销后旧数据串号。
 * 只保留UI快照身份和版本，不持有身份认证密钥或业务权威状态。
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsScopedDomainViewModelBase
    : public UDivineBeastsViewModelBase
{
    GENERATED_BODY()
public:
    /** 设置当前已认证UI数据来源代次，重复设置相同代次不触发清空。 */
    bool BeginSourceScope(const FGuid& NewScopeId);
    /** 离开账号/数据源时撤销快照，旧回调将不再匹配新作用域。 */
    void ClearSourceScope();

    /** 当前UI数据源是否已就绪；false时蓝图须显示不可用状态。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Domain")
    bool HasActiveSourceScope() const { return SourceScopeId.IsValid(); }
    /** 最近接收的UI快照修订号，-1表示尚未收到该作用域的有效快照。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Domain")
    int64 GetSourceRevision() const { return SourceRevision; }

protected:
    /** 带来源身份、严格递增修订号的客户端快照才有资格更新页面。 */
    bool CanAcceptSnapshot(const FGuid& ScopeId, int64 InSnapshotRevision) const;
    /** 仅在派生类完成数据校验和投影赋值后广播。 */
    void PublishAcceptedSnapshot(int64 InSnapshotRevision);
    /** 切换来源时由派生类清空领域投影，避免旧账号内容短暂残留。 */
    virtual void ResetDomainProjection() {}

private:
    /** 已授权UI数据源作用域；不保存任何账号凭据与可逆身份秘密。 */
    UPROPERTY(Transient)
    FGuid SourceScopeId;
    /** 最后一次接受的来源版本；切换作用域后重置为-1。 */
    UPROPERTY(Transient)
    int64 SourceRevision = -1;
};
