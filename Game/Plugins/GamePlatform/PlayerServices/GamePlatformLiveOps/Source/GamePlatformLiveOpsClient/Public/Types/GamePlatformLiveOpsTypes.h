// 平台中立只读领域投影：后端拥有权益/成长/运营权威；游戏线程服务产生值副本，单位/空值语义如下，不保存认证秘密。
#pragma once

#include "CoreMinimal.h"
#include "GamePlatformLiveOpsTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformLiveOpsError : uint8
{
    None, // 当前操作无错误，不表示客户端获得权威写权限
    CatalogUnavailable, // 运营/商店目录不可用
    CatalogRevisionMismatch, // 目录要求版本与后端不一致
    CampaignNotFound, // 后端不存在签到活动身份
    CampaignNotActive, // 当前权威UTC窗口不允许签到
    NotEligible, // 当前玩家不满足后端资格
    AlreadyClaimed, // 本周期已经领取，不能再次发奖
    ClaimInProgress, // 同一领奖操作尚未终态
    DuplicateOperation, // 操作身份已登记，后端须幂等返回原结果
    InvalidPeriod, // 签到周期身份与后端不一致
    RewardPreflightFailed, // 后端奖励发布预检失败
    RewardGrantFailed, // 后端奖励应用失败，客户端不能补发
    OutcomeUnknown, // 请求可能已提交，必须按原操作/订单身份查询对账
    BackendUnavailable, // 领域传输/后端不可用，保留可读投影
    Unauthorized, // 认证失效或调用方权限不足
    Cancelled, // 仅本地等待取消，不能回滚已提交的权威事务
    TimedOut, // 等待截止时间已到，是否提交依终态与原操作对账
    InvalidResponse // 响应结构/身份/版本无法验证，不能替换旧快照
};

UENUM(BlueprintType)
enum class EGamePlatformLiveOpsClientState : uint8
{
    Uninitialized,
    Loading,
    Ready,
    Reconciling,
    Error
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsTimeWindow
{
    GENERATED_BODY()

    /** 活动开始UTC；展示窗口含起点，采用后端UTC时间样本。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime StartsAtUtc;

    /** 是否声明活动结束UTC；false表示无结束限制。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bHasEnd = false;

    /** 可选活动结束UTC；窗口不含终点，仅bHasEnd=true时生效。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime EndsAtUtc;

    bool IsActive(const FDateTime& NowUtc) const
    {
        return NowUtc >= StartsAtUtc &&
               (!bHasEnd || NowUtc < EndsAtUtc);
    }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsSeason
{
    GENERATED_BODY()

    /** 后端赛季稳定身份；None未绑定，不构造独立服务器角色。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName SeasonId = NAME_None;

    /** 后端定义正整数版本；默认1，客户端不发布运营目录。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 Version = 1;

    /** 活动名称本地化键；空值未提供，界面应使用可读回退。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString NameKey;

    /** 发布活动的UTC窗口；只派生可见性，不在客户端授予签到周期/奖励。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FGamePlatformLiveOpsTimeWindow TimeWindow;

    /** 可选中立表现元数据身份；None无资源映射，不影响权威奖励。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName PresentationMetadataId = NAME_None;

    /** 活动展示排序优先级整数；0默认，不影响服务器奖励或规则资格。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsEvent
{
    GENERATED_BODY()

    /** 后端运营事件稳定身份；None无效，不是网络复制或权威事件句柄。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName EventId = NAME_None;

    /** 运营事件展示语义类型；None未声明，不在客户端执行领域规则。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName EventType = NAME_None;

    /** 后端赛季稳定身份；None未绑定，不构造独立服务器角色。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName SeasonId = NAME_None;

    /** 发布活动的UTC窗口；只派生可见性，不在客户端授予签到周期/奖励。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FGamePlatformLiveOpsTimeWindow TimeWindow;

    /** 可选中立表现元数据身份；None无资源映射，不影响权威奖励。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName PresentationMetadataId = NAME_None;

    /** 活动展示排序优先级整数；0默认，不影响服务器奖励或规则资格。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 Priority = 0;

    /** 只读运营展示标签集合；空集合合法，不充当服务器权限。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    TArray<FName> Tags;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsSignInCampaign
{
    GENERATED_BODY()

    /** 后端签到活动稳定身份；None无活动，Claim必须提供有效身份。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName CampaignId = NAME_None;

    /** 后端定义正整数版本；默认1，客户端不发布运营目录。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 Version = 1;

    /** 发布活动的UTC窗口；只派生可见性，不在客户端授予签到周期/奖励。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FGamePlatformLiveOpsTimeWindow TimeWindow;

    /** 可选中立表现元数据身份；None无资源映射，不影响权威奖励。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName PresentationMetadataId = NAME_None;

    /** 后端发布奖励项目数，非负整数；0无项目，不表示奖励已经授予。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 RewardCount = 0;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsCatalogSnapshot
{
    GENERATED_BODY()

    /** 目录格式版本；0未知，客户端只接纳已支持的后端格式。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 CatalogVersion = 0;

    /** 后端目录单调修订号；0未加载，与本地ViewGeneration分离。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int64 CatalogRevision = 0;

    /** 运营目录发布UTC时刻；不据本地时钟推断发布成功。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime PublishedAtUtc;

    /** 服务器UTC样本；用于展示时间估算，客户端不自行确定权威签到周期。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime ServerTimeUtc;

    /** 已发布赛季只读集合；空集合合法，不是客户端赛季权威。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    TArray<FGamePlatformLiveOpsSeason> Seasons;

    /** 已发布运营事件只读集合；空集合合法，展示不负责发奖。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    TArray<FGamePlatformLiveOpsEvent> Events;

    /** 已发布签到活动定义集合；玩家实际领取状态来自CampaignStates。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    TArray<FGamePlatformLiveOpsSignInCampaign> SignInCampaigns;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsCampaignState
{
    GENERATED_BODY()

    /** 后端签到活动稳定身份；None无活动，Claim必须提供有效身份。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName CampaignId = NAME_None;

    /** 后端当前签到周期不透明身份；客户端不能在跨UTC边界时自行推进。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString CurrentPeriodKey;

    /** 后端是否确认当前周期已领取；默认false，不能在按钮点击后本地置true。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bClaimedCurrentPeriod = false;

    /** 后端确认的累计领取次数非负整数；客户端不自增。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 TotalClaimCount = 0;

    /** 后端下一奖励从0开始的索引；0初始，不意味着奖励可立即领取。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 NextRewardIndex = 0;

    /** 后端奖励/履约机器状态；空值未知，不按自然语言文本推断成功。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString RewardStatus;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsPlayerState
{
    GENERATED_BODY()

    /** 后端玩家运营状态单调版本；0未加载，账号重置恢复0。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int64 PlayerStateRevision = 0;

    /** 后端生成玩家快照UTC；有效响应需有实际时间。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime GeneratedAtUtc;

    /** 服务器UTC样本；用于展示时间估算，客户端不自行确定权威签到周期。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime ServerTimeUtc;

    /** 当前账号活动状态集合；账号切换全部清空，不向其他账号复用。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    TArray<FGamePlatformLiveOpsCampaignState> CampaignStates;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsClaimResult
{
    GENERATED_BODY()

    /** 后端领取记录唯一身份；空值没有有效领取记录。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString ClaimId;

    /** 后端回显的原幂等领奖操作身份；未知结果按此查询，不创建新身份重复发奖。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString ClaimOperationId;

    /** 后端签到活动稳定身份；None无活动，Claim必须提供有效身份。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName CampaignId = NAME_None;

    /** 领取所属的后端签到周期不透明身份；客户端不重写当前周期。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString PeriodKey;

    /** 后端领取操作机器状态；空值未知，只有权威结果能说明奖励进展。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString Status;

    /** 后端玩家运营状态单调版本；0未加载，账号重置恢复0。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int64 PlayerStateRevision = 0;

    /** 服务器UTC样本；用于展示时间估算，客户端不自行确定权威签到周期。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FDateTime ServerTimeUtc;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsEventViewModel
{
    GENERATED_BODY()

    /** 后端运营事件稳定身份；None无效，不是网络复制或权威事件句柄。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName EventId = NAME_None;

    /** 按后端UTC样本/活动窗口派生的展示活跃标志；默认false，不替代奖励资格。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bActive = false;

    /** 到开始的剩余秒数double，显示侧夹到非负；0表示已开始或无可用时间样本。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    double TimeUntilStartSeconds = 0.0;

    /** 到结束的剩余秒数double；0表示已结束/无截止或无样本，结合活动窗口解释。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    double TimeUntilEndSeconds = 0.0;

    /** 可选中立表现元数据身份；None无资源映射，不影响权威奖励。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName PresentationMetadataId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsSignInViewModel
{
    GENERATED_BODY()

    /** 后端签到活动稳定身份；None无活动，Claim必须提供有效身份。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FName CampaignId = NAME_None;

    /** 按后端UTC样本/活动窗口派生的展示活跃标志；默认false，不替代奖励资格。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bActive = false;

    /** 依据后端状态与活动窗口派生的按钮资格；默认false，服务器仍需验证。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bClaimable = false;

    /** 后端是否确认当前周期已领取；默认false，不能在按钮点击后本地置true。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    bool bClaimedCurrentPeriod = false;

    /** 后端确认的累计领取次数非负整数；客户端不自增。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 TotalClaimCount = 0;

    /** 后端下一奖励从0开始的索引；0初始，不意味着奖励可立即领取。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    int32 NextRewardIndex = 0;

    /** 到开始的剩余秒数double，显示侧夹到非负；0表示已开始或无可用时间样本。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    double TimeUntilStartSeconds = 0.0;

    /** 到结束的剩余秒数double；0表示已结束/无截止或无样本，结合活动窗口解释。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    double TimeUntilEndSeconds = 0.0;

    /** 后端奖励/履约机器状态；空值未知，不按自然语言文本推断成功。 */
    UPROPERTY(BlueprintReadOnly, Category="LiveOps")
    FString RewardStatus;
};
