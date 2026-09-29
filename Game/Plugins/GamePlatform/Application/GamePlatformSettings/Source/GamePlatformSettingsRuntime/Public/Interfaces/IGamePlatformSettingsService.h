#pragma once

#include "Types/GamePlatformSettingTypes.h"

class UGameInstance;
class UObject;

using FGamePlatformSettingsRuntimeChangedCallback =
    TFunction<void(
        const FGamePlatformSettingsChangeSet&,
        const FGamePlatformSettingsSnapshot&)>;

/**
 * 游戏平台统一设置访问服务。
 *
 * Runtime 负责 Registry、分层解析、校验、Snapshot、ChangeSet、迁移和持久化编排；
 * 具体 Input/UI/Camera/SFX 领域仍由对应插件消费设置变化，不在此处复制其实现。
 */
class GAMEPLATFORMSETTINGSRUNTIME_API IGamePlatformSettingsService
{
public:
    virtual ~IGamePlatformSettingsService() = default;

    static IGamePlatformSettingsService* Get(UGameInstance& GameInstance);

    /** O(1) 读取当前已 Apply 快照。 */
    virtual bool GetValue(
        FName SettingId,
        FGamePlatformSettingValue& OutValue) const = 0;

    /** 返回稳定 Descriptor；仅供设置页、诊断和上层适配，不返回可变注册表。 */
    virtual bool GetDescriptor(
        FName SettingId,
        FGamePlatformSettingDescriptor& OutDescriptor) const = 0;

    /** 写入指定覆盖层的内存值并标记 Dirty；不 Apply、不写盘。 */
    virtual FGamePlatformResult SetValue(
        FName SettingId,
        EGamePlatformSettingLayer Layer,
        const FGamePlatformSettingValue& Value) = 0;

    /** 删除指定层的一个覆盖；不影响其他层。 */
    virtual FGamePlatformResult ResetValue(
        FName SettingId,
        EGamePlatformSettingLayer Layer) = 0;

    /** 批量删除分类在指定层的覆盖；一次 Apply 后只产生一个 ChangeSet。 */
    virtual FGamePlatformResult ResetCategory(
        FName Category,
        EGamePlatformSettingLayer Layer) = 0;

    /** 解析全部层、生成新 Snapshot 并批量广播真实变化；0 Tick、0 磁盘 IO。 */
    virtual FGamePlatformResult Apply(
        EGamePlatformSettingsChangeReason Reason =
            EGamePlatformSettingsChangeReason::Apply) = 0;

    /** 异步保存 User 层；不会因 Slider 每次 ValueChanged 自动写盘。 */
    virtual FGamePlatformResult Save() = 0;

    /** 重新发现 Provider、读取持久层、执行逐版本迁移并生成 Snapshot。 */
    virtual FGamePlatformResult Reload() = 0;

    /**
     * 切换客户端User Profile上下文并Reload。
     * UserContextKey是不透明稳定键；空键表示Logout。脏数据、待Apply或异步Save存在时拒绝切换。
     * 服务器端不支持本接口，且基础层不依赖Online/账号系统。
     */
    virtual FGamePlatformResult SwitchUserContext(const FString& UserContextKey) = 0;

    virtual FGamePlatformSettingsSnapshot GetSnapshot() const = 0;
    virtual FGamePlatformSettingsRuntimeDiagnostics GetDiagnostics() const = 0;

    /** 成功后立即回调当前空 ChangeSet + Snapshot，之后仅 Apply/Reload 等真实变化通知。 */
    virtual FGamePlatformSettingsRuntimeSubscription Subscribe(
        TWeakObjectPtr<UObject> Owner,
        FGamePlatformSettingsRuntimeChangedCallback Callback,
        FGamePlatformResult& OutResult) = 0;

    virtual bool Unsubscribe(
        const FGamePlatformSettingsRuntimeSubscription& Subscription) = 0;
};
