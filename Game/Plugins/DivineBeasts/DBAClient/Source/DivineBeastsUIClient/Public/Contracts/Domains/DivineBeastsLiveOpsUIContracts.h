#pragma once

#include "CoreMinimal.h"
#include "DivineBeastsLiveOpsUIContracts.generated.h"

/** 项目运营通知条目；奖励与商业权益只引用业务标识，不在Widget内决定发放。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsLiveOpsNoticeView
{
    GENERATED_BODY()
    /** 服务端发布的公告身份，不是商品订单或领奖凭据。 */
    UPROPERTY(BlueprintReadOnly) FName NoticeId = NAME_None;
    /** 已本地化的可见标题。 */
    UPROPERTY(BlueprintReadOnly) FText Title;
    /** 已本地化的正文摘要。 */
    UPROPERTY(BlueprintReadOnly) FText Summary;
    /** 该条公告在当前账号视角是否未读。 */
    UPROPERTY(BlueprintReadOnly) bool bUnread = false;
};

/** 邮件、活动及公告的只读展示投影；商城订单由独立GamePlatformCommerceUI处理。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsLiveOpsUIProjection
{
    GENERATED_BODY()
    /** 当前账号/服务绑定代次，注销后失效。 */
    UPROPERTY(BlueprintReadOnly) FGuid SourceScopeId;
    /** 本次运营事实版本；-1表示没有数据。 */
    UPROPERTY(BlueprintReadOnly) int64 Revision = -1;
    /** 没有合法服务适配器时为false，UI需要展示不可用状态。 */
    UPROPERTY(BlueprintReadOnly) bool bServiceAvailable = false;
    /** 服务端确认的未读总数，必须大于等于0。 */
    UPROPERTY(BlueprintReadOnly) int32 UnreadCount = 0;
    /** 当前页公告摘要，单次最多128项，不等于完整运营事件列表。 */
    UPROPERTY(BlueprintReadOnly) TArray<FDivineBeastsLiveOpsNoticeView> Notices;
};
