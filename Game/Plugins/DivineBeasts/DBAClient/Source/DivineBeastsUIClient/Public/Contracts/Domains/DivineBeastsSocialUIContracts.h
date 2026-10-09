#pragma once

#include "CoreMinimal.h"
#include "DivineBeastsSocialUIContracts.generated.h"

/** FDivineBeastsSocialMemberView（社交成员展示快照），无联系方式和认证信息。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsSocialMemberView
{
    GENERATED_BODY()
    /** 公开玩家展示身份，不作为服务端操作授权依据。 */
    UPROPERTY(BlueprintReadOnly) FString PublicPlayerId;
    /** 已经由客户端隐私策略过滤后的展示昵称。 */
    UPROPERTY(BlueprintReadOnly) FText DisplayName;
    /** 用户授权可见的在线状态；不得从缺失的网络事件猜测在线。 */
    UPROPERTY(BlueprintReadOnly) bool bOnline = false;
    /** 当前成员是否实际属于本人队伍。 */
    UPROPERTY(BlueprintReadOnly) bool bPartyMember = false;
};

/**
 * FDivineBeastsSocialUIProjection（社交与队伍只读UI投影）。
 * 必须由有权限的社交客户端适配器生产；名单排序、隐私和权限由业务层决定。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsSocialUIProjection
{
    GENERATED_BODY()
    /** 对应已认证客户端数据源的随机作用域身份；换账号后必须不同。 */
    UPROPERTY(BlueprintReadOnly) FGuid SourceScopeId;
    /** 数据源递增版本；-1表示没有可接受的快照。 */
    UPROPERTY(BlueprintReadOnly) int64 Revision = -1;
    /** false明确表示当前账号无可展示的社交服务，不能伪造在线列表。 */
    UPROPERTY(BlueprintReadOnly) bool bServiceAvailable = false;
    /** 公开队伍身份；不充当队伍管理权限证明。 */
    UPROPERTY(BlueprintReadOnly) FName PartyId = NAME_None;
    /** 当前页队伍成员视图；客户端限制最多10人。 */
    UPROPERTY(BlueprintReadOnly) TArray<FDivineBeastsSocialMemberView> PartyMembers;
    /** 当前页好友列表快照；客户端最多保存256项。 */
    UPROPERTY(BlueprintReadOnly) TArray<FDivineBeastsSocialMemberView> Friends;
};
