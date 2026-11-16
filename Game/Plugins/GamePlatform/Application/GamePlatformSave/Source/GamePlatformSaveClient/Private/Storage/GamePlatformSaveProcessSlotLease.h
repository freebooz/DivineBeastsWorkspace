#pragma once

// GamePlatformSave（平台本地存档）私有跨GameInstance物理槽位租约。
// 当前Client进程可能在注销旧GameInstance时仍运行其已提交的线程池IO；
// 仅在Subsystem实例上保存BusyStorageIds无法阻止新实例覆盖同一主档或.bak。
// 租约属于本次实际IO的共享所有权，最后一个持有者释放时才重新开放槽位。
// 这是进程内保护，不宣称提供多进程文件锁或服务器权威存储事务。

#include "CoreMinimal.h"
#include "Containers/Set.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Templates/SharedPointer.h"

class FGamePlatformSaveProcessSlotLease final
{
public:
    /** 全进程互斥占有同一个稳定摘要槽位；重复/空槽位返回空，不等待也不抢占。 */
    static TSharedPtr<FGamePlatformSaveProcessSlotLease, ESPMode::ThreadSafe> TryAcquire(
        const FString& StableStorageId)
    {
        if (StableStorageId.IsEmpty())
        {
            return nullptr;
        }

        FScopeLock Lock(&Mutex());
        if (ActiveStorageIds().Contains(StableStorageId))
        {
            return nullptr;
        }

        // 临时锁仅保护槽位注册，禁止在持锁期间执行文件IO或外部回调。
        TSharedPtr<FGamePlatformSaveProcessSlotLease, ESPMode::ThreadSafe> Lease =
            MakeShared<FGamePlatformSaveProcessSlotLease, ESPMode::ThreadSafe>(StableStorageId);
        ActiveStorageIds().Add(StableStorageId);
        return Lease;
    }

    /** 任何最后持有者线程均可归还，仅释放本槽位而不修改其它请求。 */
    ~FGamePlatformSaveProcessSlotLease()
    {
        FScopeLock Lock(&Mutex());
        ensureMsgf(ActiveStorageIds().Remove(StableStorageId) == 1,
            TEXT("存档进程槽位租约释放必须与一次成功申请对应。"));
    }

    // 构造仅由TryAcquire经稳定摘要调用，实例创建不自行占用全局集合。
    explicit FGamePlatformSaveProcessSlotLease(FString InStableStorageId)
        : StableStorageId(MoveTemp(InStableStorageId))
    {
    }

    FGamePlatformSaveProcessSlotLease(const FGamePlatformSaveProcessSlotLease&) = delete;
    FGamePlatformSaveProcessSlotLease& operator=(const FGamePlatformSaveProcessSlotLease&) = delete;

private:
    static FCriticalSection& Mutex()
    {
        static FCriticalSection Lock;
        return Lock;
    }

    static TSet<FString>& ActiveStorageIds()
    {
        static TSet<FString> Ids;
        return Ids;
    }

    FString StableStorageId;
};
