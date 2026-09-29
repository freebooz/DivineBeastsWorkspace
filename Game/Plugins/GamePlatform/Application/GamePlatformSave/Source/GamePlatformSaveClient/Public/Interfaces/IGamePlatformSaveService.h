#pragma once

// GamePlatformSave稳定C++服务契约。
// 调用方只看到逻辑Key、二进制记录和异步结果，不接触文件系统实现。
#include "Interfaces/IGamePlatformSaveProvider.h"
#include "Types/GamePlatformSaveTypes.h"

class UGameInstance;

/** 异步完成回调；始终在游戏线程调用，回调期间禁止同步反入Save服务。 */
using FGamePlatformSaveCallback =
    TFunction<void(const FGamePlatformSaveOperationResult&)>;

/**
 * GamePlatformSave客户端服务接口。
 *
 * 作用域：GameInstance（游戏实例），跨地图保持。
 * 线程：所有公开接口只能在游戏线程调用；磁盘IO内部异步执行。
 * 权威：只保存可丢失、可由其他真源重建的本地非权威数据。
 */
class GAMEPLATFORMSAVECLIENT_API IGamePlatformSaveService
{
public:
    virtual ~IGamePlatformSaveService() = default;

    /** 获取指定GameInstance的Save服务；专服、命令行或未创建服务时返回nullptr。 */
    static IGamePlatformSaveService* Get(UGameInstance& GameInstance);

    /** 注册一个唯一Namespace Provider；重复命名空间Fail Closed。 */
    virtual FGamePlatformResult RegisterProvider(
        const TSharedRef<IGamePlatformSaveProvider>& Provider) = 0;

    /** 注销命名空间；进行中的Load持有自己的Provider快照，不受注销影响。 */
    virtual FGamePlatformResult UnregisterProvider(FName Namespace) = 0;

    /**
     * 异步保存记录。
     * 启动成功返回有效Handle；同一逻辑Key已有IO时拒绝，避免Load/Save/Delete竞态。
     */
    virtual FGamePlatformSaveRequestHandle SaveRecordAsync(
        const FGamePlatformSaveRecord& Record,
        FGamePlatformSaveCallback Callback,
        FGamePlatformResult& OutStartResult) = 0;

    /**
     * 异步加载记录。
     * 主档失败后自动尝试.bak；若注册Provider且存档版本较旧，在游戏线程执行迁移。
     */
    virtual FGamePlatformSaveRequestHandle LoadRecordAsync(
        const FGamePlatformSaveKey& Key,
        FGamePlatformSaveCallback Callback,
        FGamePlatformResult& OutStartResult) = 0;

    /** 异步删除主档和备份；记录不存在视为幂等成功。 */
    virtual FGamePlatformSaveRequestHandle DeleteRecordAsync(
        const FGamePlatformSaveKey& Key,
        FGamePlatformSaveCallback Callback,
        FGamePlatformResult& OutStartResult) = 0;

    /** 返回轻量诊断；不触发磁盘读取。 */
    virtual FGamePlatformSaveDiagnostics GetDiagnostics() const = 0;
};
