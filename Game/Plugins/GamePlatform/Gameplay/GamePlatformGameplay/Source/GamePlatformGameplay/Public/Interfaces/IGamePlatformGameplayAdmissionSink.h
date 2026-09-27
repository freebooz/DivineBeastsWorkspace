#pragma once
#include "Interfaces/IGamePlatformGameplayService.h"
#include "Types/GamePlatformSpawnRequest.h"

class APlayerController;

/**
 * 服务器组合根持有的可信登记读取器，查询已有已提交准入及撤销事实。
 * 只允许游戏线程有界内存查询，不能阻塞网络；未确认、过期或后端状态未知必须非成功。
 * 此接口没有默认放行实现，Gameplay不根据客户端Session快照自行认证。
 */
class GAMEPLATFORMGAMEPLAY_API IGamePlatformGameplayAdmissionAuthority
{
public:
    virtual ~IGamePlatformGameplayAdmissionAuthority() = default;
    /** 校验真实Controller连接、完整上下文、实例启动代次和当前有效期限；每个权威动作会重新查询。 */
    virtual FGamePlatformResult ValidateCurrentAdmission(const APlayerController& Controller,
        const FGamePlatformVerifiedPlayerContext& Context) const = 0;
};

/** 服务器GameMode提供的C++接收边界；不是RPC，不暴露任意设置认证状态的蓝图函数。 */
class GAMEPLATFORMGAMEPLAY_API IGamePlatformGameplayAdmissionSink
{
public:
    virtual ~IGamePlatformGameplayAdmissionSink() = default;
    /** 仅启动前注册一个可信来源，Owner需在本世界存活；重复注册拒绝。 */
    virtual FGamePlatformGameplayRegistration RegisterAdmissionAuthority(TWeakObjectPtr<UObject> Owner,
        TSharedRef<IGamePlatformGameplayAdmissionAuthority> Authority, FGamePlatformResult& OutResult) = 0;
    /** 精确撤销当前来源；同时使本世界全部玩家资格失效并清理自身对象。 */
    virtual bool UnregisterAdmissionAuthority(const FGamePlatformGameplayRegistration& Registration) = 0;
    /** 只接收与当前Controller连接绑定且经Authority复核的上下文；重复同Admission幂等，冲突拒绝。 */
    virtual FGamePlatformResult SubmitVerifiedAdmission(APlayerController& Controller,
        const FGamePlatformVerifiedPlayerContext& Context) = 0;
    /** 仅精确当前AdmissionId/ConnectionGeneration/SessionEpoch可撤销；旧事件返回失败。 */
    virtual FGamePlatformResult RevokeVerifiedAdmission(APlayerController& Controller,
        const FGamePlatformAdmissionRevocation& Revocation) = 0;
    /** 重新采样基础世界、体验、准入和Pawn事实；Allowed不代表已经出生。 */
    virtual FGamePlatformSpawnEligibility EvaluatePlayerStartEligibility(APlayerController& Controller) const = 0;
    /** 仅服务器C++调用；需定义允许及当前有效准入，旧Pawn先清理，再创建新出生代次。 */
    virtual FGamePlatformResult RequestServerRestart(APlayerController& Controller) = 0;
};
