// MOBA层客户端适配端口的跨DLL生命周期与安全默认行为；不联网、不拥有队列/World/玩家权威。
// 项目层组合根可提供真实实现；旧实现没有完整合同仍明确失败，不以固定成功冒充匹配。
#include "Client/GamePlatformArenaMatchmakingAdapter.h"

FGamePlatformArenaMatchOffer::FGamePlatformArenaMatchOffer() = default;
IGamePlatformArenaMatchmakingAdapter::IGamePlatformArenaMatchmakingAdapter() = default;
IGamePlatformArenaMatchmakingAdapter::~IGamePlatformArenaMatchmakingAdapter() = default;

bool IGamePlatformArenaMatchmakingAdapter::SupportsTicketLifecycle() const
{
    return false;
}

FGamePlatformArenaMatchOffer IGamePlatformArenaMatchmakingAdapter::GetObservedOffer() const
{
    return FGamePlatformArenaMatchOffer();
}

void IGamePlatformArenaMatchmakingAdapter::RespondToMatchOffer(const FGamePlatformArenaMatchOffer&,bool,
    const FString&,TFunction<void(bool,const FString&)> Completion)
{
    if(Completion)Completion(false,TEXT("Matchmaking.OfferContractUnavailable"));
}
