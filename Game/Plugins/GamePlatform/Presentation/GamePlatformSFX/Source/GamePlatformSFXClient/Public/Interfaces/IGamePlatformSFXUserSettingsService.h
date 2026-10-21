// 平台音频领域的设置入口。调用方只投影快照/提交命令，不保存第二份INI或创建播放器。
#pragma once
#include "Types/GamePlatformResult.h"
#include "Types/GamePlatformSFXUserSettingsTypes.h"
class UGameInstance;

/** 全接口游戏线程；GameInstance作用域跨World保存，Dedicated Server/命令行不提供实例。 */
class GAMEPLATFORMSFXCLIENT_API IGamePlatformSFXUserSettingsService
{
public:
    virtual ~IGamePlatformSFXUserSettingsService() = default;
    static IGamePlatformSFXUserSettingsService* Get(UGameInstance& GameInstance);
    /** 绑定前读取一次完整快照；之后仅通过变化通知更新，不轮询。 */
    virtual FGamePlatformSFXUserVolumes GetSnapshot() const = 0;
    /** Value为0..1有限线性值；即时应用并发布，重复值不重复通知。捕获结束/页面退出须调用CommitVolumes。 */
    virtual FGamePlatformResult SetVolume(EGamePlatformSFXSoundGroup Group, float Value) = 0;
    /** 仅脏值写本地磁盘；失败保留运行值并返回真实错误，后续可重试。不回滚显示设置。 */
    virtual FGamePlatformResult CommitVolumes() = 0;
    /** 不同步重发初始快照；调用方应使用弱绑定并在作用域退出精确解绑。通知期间禁止修改设置。 */
    virtual FDelegateHandle AddChangedHandler(const FGamePlatformSFXUserVolumesChanged::FDelegate& Handler) = 0;
    virtual void RemoveChangedHandler(FDelegateHandle Handle) = 0;
};
