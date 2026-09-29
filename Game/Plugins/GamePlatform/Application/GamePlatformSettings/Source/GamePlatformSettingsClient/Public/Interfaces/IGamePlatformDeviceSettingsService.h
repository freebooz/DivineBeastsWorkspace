#pragma once

#include "Types/GamePlatformDeviceSettingsTypes.h"

class UGameInstance;
class UObject;

/** 低频设置状态回调；调用期间禁止同步反入修改服务。 */
using FGamePlatformDeviceSettingsChangedCallback =
    TFunction<void(const FGamePlatformDeviceSettingsSnapshot&)>;

/**
 * GamePlatformSettings 客户端设备设置适配服务契约。
 *
 * 作用域：GameInstance（游戏实例），因为设备级显示与画质设置跨地图存在。
 * 线程：全部接口仅游戏线程。
 * 权威：仅影响本地客户端设备，不得改变服务器权威玩法结果。
 */
class GAMEPLATFORMSETTINGSCLIENT_API IGamePlatformDeviceSettingsService
{
public:
    virtual ~IGamePlatformDeviceSettingsService() = default;

    /** 取得当前 GameInstance 的设置服务；专服、命令行或未创建服务时返回 nullptr。 */
    static IGamePlatformDeviceSettingsService* Get(UGameInstance& GameInstance);

    /** 返回只读快照；不会触发磁盘 IO。 */
    virtual FGamePlatformDeviceSettingsSnapshot GetSnapshot() const = 0;

    /** 返回轻量诊断；不会读取磁盘或扫描资产。 */
    virtual FGamePlatformDeviceSettingsDiagnostics GetDiagnostics() const = 0;

    /** 从磁盘重新加载并应用 UGameUserSettings；预览期间拒绝执行。 */
    virtual FGamePlatformResult ReloadFromDisk() = 0;

    /** 校验并暂存候选设备设置，不应用、不写盘。 */
    virtual FGamePlatformResult StageDeviceSettings(
        const FGamePlatformDeviceSettings& Settings) = 0;

    /**
     * 应用已暂存设置。
     * Preview：应用但不写盘，后续必须 ConfirmPreview 或 CancelPreview。
     * Commit：应用、确认显示模式并一次性保存。
     */
    virtual FGamePlatformResult ApplyStagedSettings(
        EGamePlatformDeviceSettingsApplyMode ApplyMode) = 0;

    /** 确认当前预览并一次性保存；非预览状态调用失败。 */
    virtual FGamePlatformResult ConfirmPreview() = 0;

    /** 回滚到预览前完整设置；不写盘。 */
    virtual FGamePlatformResult CancelPreview() = 0;

    /** 丢弃尚未应用的暂存值，恢复为当前值；预览期间拒绝。 */
    virtual FGamePlatformResult DiscardStagedSettings() = 0;

    /** 成功订阅后立即回调一次当前快照，之后只在快照真实变化时通知。 */
    virtual FGamePlatformDeviceSettingsSubscription Subscribe(
        TWeakObjectPtr<UObject> Owner,
        FGamePlatformDeviceSettingsChangedCallback Callback,
        FGamePlatformResult& OutResult) = 0;

    /** 精确撤销订阅；跨实例、旧代次或重复撤销返回 false。 */
    virtual bool Unsubscribe(
        const FGamePlatformDeviceSettingsSubscription& Subscription) = 0;
};
