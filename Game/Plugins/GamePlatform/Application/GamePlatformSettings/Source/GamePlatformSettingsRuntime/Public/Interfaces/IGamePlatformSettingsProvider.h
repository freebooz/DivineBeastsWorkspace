// 平台共享设置Provider契约：上层注入描述/迁移，Runtime不反向依赖领域；Client持久化实例按GI独占，Server只读适配无用户状态。
#pragma once

#include "Features/IModularFeature.h"
#include "Types/GamePlatformSettingTypes.h"

/**
 * 上层设置描述 Provider。
 * MobaCommon/DivineBeasts 只实现并注册自己的 Provider，平台层不反向依赖它们。
 */
class GAMEPLATFORMSETTINGSRUNTIME_API IGamePlatformSettingsProvider
    : public IModularFeature
{
public:
    virtual ~IGamePlatformSettingsProvider() = default;

    static FName GetModularFeatureName();

    /** 全进程唯一 ProviderId，用于冲突诊断和确定性排序。 */
    virtual FName GetProviderId() const = 0;

    /** 批量返回本 Provider 拥有的 Descriptor；不得返回其他 Provider 的设置。 */
    virtual void GetSettingDescriptors(
        TArray<FGamePlatformSettingDescriptor>& OutDescriptors) const = 0;
};

/** 持久化载荷；Runtime 私有解析层与 Client/Server IO 适配之间的纯值边界。 */
struct FGamePlatformSettingsPersistencePayload
{
    int32 SchemaVersion = 1;
    TMap<EGamePlatformSettingLayer, TMap<FName, FGamePlatformSettingValue>> Layers;
};

using FGamePlatformSettingsSaveCompletion =
    TFunction<void(const FGamePlatformResult&)>;

/**
 * 持久化 Provider。
 * Client 可实现本地用户档案；Server 可实现 INI/环境变量/命令行只读层。
 */
class GAMEPLATFORMSETTINGSRUNTIME_API IGamePlatformSettingsPersistenceProvider
    : public IModularFeature
{
public:
    virtual ~IGamePlatformSettingsPersistenceProvider() = default;

    /** 创建供一个GI独占的Provider；进程注册对象只做工厂。Client必须实现；默认空表示不支持并由Runtime明确拒绝。
     * 克隆应无用户键，GI负责SetUserContext；析构不回写其他GI。Server只读Provider允许共享。 */
    virtual TUniquePtr<IGamePlatformSettingsPersistenceProvider> CreateScopedProvider() const { return nullptr; }

    static FName GetModularFeatureName();
    virtual FName GetPersistenceId() const = 0;
    virtual bool SupportsRuntime(EGamePlatformSettingRuntimeScope RuntimeScope) const = 0;

    /**
     * 切换本GI独占Provider的本地用户上下文。基础层只接收不透明稳定键，不依赖Online/账号类型。
     * 空键表示无登录用户；Server实现应返回Unsupported。
     */
    virtual FGamePlatformResult SetUserContext(const FString& UserContextKey) = 0;

    /**
     * 读取配置层。实现必须只填充自己拥有的持久化/部署层；
     * Runtime 会再次执行 Descriptor/类型/端侧校验。
     */
    virtual FGamePlatformResult Load(
        const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
        FGamePlatformSettingsPersistencePayload& OutPayload) = 0;

    /**
     * 异步保存 User 层；Server/不支持持久化的实现返回 Unsupported。
     * 仅游戏线程调用/完成；接纳成功只表示IO已启动，最终结果通过Completion恰一次回调。
     * 返回失败/Unsupported不调用Completion；启动时复制纯值与用户槽身份。Provider退出后在飞IO不得访问裸this或GI，取消本地消费者不回滚磁盘结果。
     */
    virtual FGamePlatformResult BeginSave(
        const TMap<FName, FGamePlatformSettingValue>& UserValues,
        int32 SchemaVersion,
        FGamePlatformSettingsSaveCompletion Completion) = 0;
};

/** 单步版本迁移接口；每次只允许 FromVersion -> FromVersion+1。 */
class GAMEPLATFORMSETTINGSRUNTIME_API IGamePlatformSettingsMigration
    : public IModularFeature
{
public:
    virtual ~IGamePlatformSettingsMigration() = default;

    static FName GetModularFeatureName();
    virtual FName GetMigrationId() const = 0;
    virtual int32 GetFromVersion() const = 0;
    virtual int32 GetToVersion() const = 0;

    /** 只迁移 User 层纯值，不直接操作 UObject、磁盘或其他插件私有对象。 */
    virtual FGamePlatformResult Migrate(
        TMap<FName, FGamePlatformSettingValue>& InOutUserValues) const = 0;
};
