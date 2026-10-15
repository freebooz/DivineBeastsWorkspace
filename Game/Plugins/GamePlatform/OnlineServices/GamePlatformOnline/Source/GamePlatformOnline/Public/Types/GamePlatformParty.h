// 平台层普通队伍公开值合同：供在线客户端与项目UI消费；不包含竞技、地图、凭据或网络权威写权限。
#pragma once

#include "CoreMinimal.h"
#include "GamePlatformParty.generated.h"

/** 服务端授权成员；最多五人。在线和准备均来自真实响应，昵称按纯文本显示。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMONLINE_API FGamePlatformPartyMember
{
    GENERATED_BODY()
    /** 公开玩家身份，不是令牌，也不能替代服务端授权。 */
    UPROPERTY(BlueprintReadOnly) FString PlayerId;
    /** 经服务端提供的展示名，空值时界面显示公开身份。 */
    UPROPERTY(BlueprintReadOnly) FString DisplayName;
    /** 当前名单修订下的本人准备状态。 */
    UPROPERTY(BlueprintReadOnly) bool bReady = false;
    /** 可信在线事实；不能通过缺少事件推测。 */
    UPROPERTY(BlueprintReadOnly) bool bOnline = false;
};

/** 邀请／申请只读记录；截止时刻只用于显示，权威终态仍由服务端校验。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMONLINE_API FGamePlatformPartyRequest
{
    GENERATED_BODY()
    /** 普通队伍身份；来自收件箱或当前授权队伍。 */
    UPROPERTY(BlueprintReadOnly) FString PartyId;
    /** 请求稳定身份，用于接受／拒绝／撤回。 */
    UPROPERTY(BlueprintReadOnly) FString RequestId;
    /** invitation或application，不允许未知种类。 */
    UPROPERTY(BlueprintReadOnly) FString Kind;
    /** 邀请目标或申请者的公开身份。 */
    UPROPERTY(BlueprintReadOnly) FString PlayerId;
    /** 收件箱中可见的邀请者；普通请求记录可能为空。 */
    UPROPERTY(BlueprintReadOnly) FString InviterPlayerId;
    /** pending／accepted／declined／cancelled；过期由ExpiresAtUtc判断显示，不自行改写。 */
    UPROPERTY(BlueprintReadOnly) FString State;
    /** UTC截止时刻；默认零值非法。 */
    UPROPERTY(BlueprintReadOnly) FDateTime ExpiresAtUtc;
    /** 当前队伍修订，非负int64；JSON使用十进制字符串避免精度丢失。 */
    UPROPERTY(BlueprintReadOnly) int64 Revision = 0;
    /** 邀请时真实成员数1..5；普通队内请求为0表示没有提供。 */
    UPROPERTY(BlueprintReadOnly) int32 MemberCount = 0;
};

/** 本人授权的普通队伍快照；不含其他队伍私有名单，失败不能替换成空成功。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMONLINE_API FGamePlatformPartySnapshot
{
    GENERATED_BODY()
    /** false表示后端确认本人无队伍，不表示网络失败。 */
    UPROPERTY(BlueprintReadOnly) bool bHasParty = false;
    /** 无队伍时必须为空。 */
    UPROPERTY(BlueprintReadOnly) FString PartyId;
    /** 已认证队长的公开身份；无队伍时为空。 */
    UPROPERTY(BlueprintReadOnly) FString LeaderPlayerId;
    /** 已选中立活动与服务器限时准备；空会话不授予准备弹窗资格。 */
    UPROPERTY(BlueprintReadOnly) FString ActivityId;
    UPROPERTY(BlueprintReadOnly) FString ReadyCheckId;
    UPROPERTY(BlueprintReadOnly) FDateTime ReadyDeadlineUtc;
    /** 当前聚合修订；仅在相同队伍身份内比较大小。 */
    UPROPERTY(BlueprintReadOnly) int64 Revision = 0;
    /** 锁定时禁止本地绕过匹配取消去修改名单。 */
    UPROPERTY(BlueprintReadOnly) bool bRosterLocked = false;
    /** 授权成员，最多五人，不包含待接受邀请。 */
    UPROPERTY(BlueprintReadOnly) TArray<FGamePlatformPartyMember> Members;
    /** 授权可见的请求，最多64项。 */
    UPROPERTY(BlueprintReadOnly) TArray<FGamePlatformPartyRequest> Requests;
    /** 服务端UTC时间基准；展示倒计时以单调时钟推进。 */
    UPROPERTY(BlueprintReadOnly) FDateTime ServerTimeUtc;
};

/** 普通队伍命令输入；主体由认证注入，不接收客户端自报ActorPlayerId。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMONLINE_API FGamePlatformPartyCommand
{
    GENERATED_BODY()
    /** 已读取的目标队伍；邀请／申请来自明确目标，不通过昵称猜测。 */
    UPROPERTY(BlueprintReadWrite) FString PartyId;
    /** Shared契约白名单动作；不包含竞技准备或入场确认。 */
    UPROPERTY(BlueprintReadWrite) FString Action;
    /** 邀请／踢出／转让所需目标。 */
    UPROPERTY(BlueprintReadWrite) FString TargetPlayerId;
    /** 接受／拒绝／撤回所需请求身份。 */
    UPROPERTY(BlueprintReadWrite) FString MembershipRequestId;
    /** 同一逻辑操作重试必须保留原键，最多128字符。 */
    UPROPERTY(BlueprintReadWrite) FString ClientRequestId;
    /** 已读取修订；-1表示未取得，禁止盲写。 */
    UPROPERTY(BlueprintReadWrite) int64 ExpectedRevision = -1;
    /** 只在ready动作时生效，不能修改其他成员。 */
    UPROPERTY(BlueprintReadWrite) bool bReady = false;
};

/** 类型化在线完成值；只在回调期间借用，取消本地等待不表示后端回滚。 */
struct GAMEPLATFORMONLINE_API FGamePlatformPartyResult
{
    /** 本人观察作用域与持久游标，仅观察响应提供；不作为认证或业务写权限。 */
    FString ObservationScopeId;
    FString ObservationCursor;
    /** 服务器要求完整替换快照；Changed=false表示空等待，不能清空已有显示。 */
    bool bResyncRequired = false;
    bool bChanged = false;
    /** 仅真实2xx及完整合法合同解析后为true。 */
    bool bSuccess = false;
    /** 写操作可能到达服务端但没有可信回执；界面须读回并保留原幂等键。 */
    bool bOutcomeUnknown = false;
    /** 稳定安全码，不返回任意后端正文或敏感传输信息。 */
    FName ErrorCode = NAME_None;
    /** 真实HTTP状态；本地参数错误／取消时可为0。 */
    int32 HttpStatusCode = 0;
    /** 成功的本人队伍／命令回执投影。 */
    FGamePlatformPartySnapshot Snapshot;
    /** 成功的本人邀请收件箱，最多20项。 */
    TArray<FGamePlatformPartyRequest> Invitations;
};
