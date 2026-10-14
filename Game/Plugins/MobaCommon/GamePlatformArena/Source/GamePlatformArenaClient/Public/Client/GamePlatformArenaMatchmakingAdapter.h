#pragma once

#include "CoreMinimal.h"
#include "Client/GamePlatformArenaClientTypes.h"

/** 逐人确认的公开候选身份；不是准入票据，不能包含服务器地址、签名或隐藏分。
 * Revision为非负服务端版本，DeadlineUtc为权威截止时间；未知候选不能提交确认。
 */
struct GAMEPLATFORMARENACLIENT_API FGamePlatformArenaMatchOffer
{
    FString CandidateId;
    int64 Revision = -1;
    FDateTime DeadlineUtc;
    bool bCanRespond = false;
};

/** IGamePlatformArenaMatchmakingAdapter（客户端匹配适配接口）。
 *  真正Queue/MMR/Region/Party原子匹配由Go MatchService负责；UE只调用Gateway稳定接口。
 */
class GAMEPLATFORMARENACLIENT_API IGamePlatformArenaMatchmakingAdapter
{
public:
    virtual ~IGamePlatformArenaMatchmakingAdapter() = default;

    /** 提交模式/原子队伍与逻辑幂等身份；仅游戏线程。回调true代表远端受理，不代表已分配竞技实例。
     * 适配器拥有认证、请求及观察的生命周期；必须核主体、服务端模式人数和名单版本，不接受隐藏分。
     */
    virtual void RequestMatchmaking(
        const FGamePlatformArenaMatchmakingRequest& Request,
        TFunction<void(bool bAccepted, const FString& Error)> Completion) = 0;

    /** 取消指定报名逻辑身份；只有后端确认才能回调true。本地HTTP取消或关页不能宣称已释放名单锁。 */
    virtual void CancelMatchmaking(
        const FString& ClientRequestId,
        TFunction<void(bool bCancelled, const FString& Error)> Completion) = 0;

    /** 最近实际观察到的只读状态，未知不能从页面开关合成；准入仍交唯一ApplicationFlow执行。 */
    virtual EGamePlatformArenaClientFlowState GetObservedState() const = 0;

    /** 只有创建、观察、取消和逐人确认都由真实后端支持才允许生成队列副作用。
     * 默认false保持旧适配器兼容；只有创建端点不能宣称完整生命周期可用。
     */
    virtual bool SupportsTicketLifecycle() const { return false; }

    /** 读取适配器最近收到的授权候选；未知/撤销返回空身份，不从本地流程枚举合成。 */
    virtual FGamePlatformArenaMatchOffer GetObservedOffer() const { return {}; }

    /** 每个玩家显式接受/拒绝指定候选版本；回调必须在游戏线程且恰好一次。
     * 不支持的旧适配器明确失败，绝不转调局内Ready或生成假成功。
     */
    virtual void RespondToMatchOffer(const FGamePlatformArenaMatchOffer& Offer, bool bAccept,
        const FString& ClientRequestId, TFunction<void(bool, const FString&)> Completion)
    {
        if (Completion) Completion(false, TEXT("Matchmaking.OfferContractUnavailable"));
    }
};
