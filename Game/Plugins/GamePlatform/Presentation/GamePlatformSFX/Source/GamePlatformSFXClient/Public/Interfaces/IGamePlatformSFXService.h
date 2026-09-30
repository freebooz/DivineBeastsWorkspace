#pragma once

#include "Types/GamePlatformSFXTypes.h"

class UObject;

/**
 * IGamePlatformSFXService（游戏平台音效服务）。
 * 公开接口只在游戏线程调用；Definition/声音资源加载由实现异步完成，调用方不得持有私有Subsystem。
 */
class GAMEPLATFORMSFXCLIENT_API IGamePlatformSFXService
{
public:
    virtual ~IGamePlatformSFXService() = default;

    /** 从任意有效世界上下文取得当前World的SFX服务；Dedicated Server/无World返回nullptr。 */
    static IGamePlatformSFXService* Get(const UObject* WorldContextObject);

    /** 提交播放请求；Queued表示异步Definition租约已受理。 */
    virtual FGamePlatformSFXResult Play(const FGamePlatformSFXRequest& Request) = 0;

    /** 停止句柄；FadeOutSeconds<0时使用Definition默认淡出。重复停止已进入停止流程的实例视为成功。 */
    virtual bool Stop(const FGamePlatformSFXHandle& Handle, float FadeOutSeconds = -1.0f) = 0;

    /** 按跨Presentation请求身份取消正在加载或播放的实例。 */
    // 游戏线程取消命令：合法ID即使尚无实例也记有界取消墓碑；true表示接纳取消，非当前活动数。
    virtual bool StopByRequestId(const FGuid& RequestId, float FadeOutSeconds = -1.0f) = 0;

    virtual bool IsActive(const FGamePlatformSFXHandle& Handle) const = 0;

    /** 只允许Definition白名单内的浮点参数；仅活动AudioComponent可更新。 */
    virtual bool SetFloatParameter(const FGamePlatformSFXHandle& Handle, FName Name, float Value) = 0;

    /** 设置实例线性音量倍数；调用方自行遵守业务设置与混音策略。 */
    virtual bool SetVolumeMultiplier(const FGamePlatformSFXHandle& Handle, float Value) = 0;

    /** 返回低频诊断快照，不扫描资产目录、不读取磁盘。 */
    virtual FGamePlatformSFXDiagnostics GetDiagnostics() const = 0;
};
