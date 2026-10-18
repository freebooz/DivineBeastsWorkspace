// 存档存储回归：本地临时夹具验证主档/备份超限在读取前拒绝，不测生产用户数据。
#include "Storage/GamePlatformSaveStorage.h"
#include "Policy/GamePlatformSavePolicy.h"
#include "Storage/GamePlatformSaveProcessSlotLease.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveOversizedStorageTest, "GamePlatform.Save.Storage.OversizedFile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSaveOversizedStorageTest::RunTest(const FString&)
{
    auto& Files = FPlatformFileManager::Get().GetPlatformFile();
    const FString Primary = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), FGuid::NewGuid().ToString() + TEXT(".gpsav"));
    Files.CreateDirectoryTree(*FPaths::GetPath(Primary));
    for (const FString& Path : {Primary, FGamePlatformSaveStorage::BuildBackupPath(Primary)})
    {
        // Seek生成超限夹具，测试本身不分配一个超大读取缓冲；只删除本轮随机文件。
        TUniquePtr<IFileHandle> Handle(Files.OpenWrite(*Path));
        if (!Handle) { AddError(TEXT("无法创建超限存档夹具")); return false; }
        const uint8 Byte = 0;
        if (!Handle->Seek(8 * 1024 * 1024 + 20) || !Handle->Write(&Byte, 1)) { AddError(TEXT("超限夹具写入失败")); return false; }
        Handle.Reset();
        TArray<uint8> Bytes;
        const auto Result = FGamePlatformSaveStorage::ReadFile(Path, Bytes);
        TestEqual(TEXT("主档及备份均由存储层拒绝超限"), Result.Code, FName(TEXT("SaveFileTooLarge")));
        TestEqual(TEXT("拒绝后没有载荷缓冲"), Bytes.Num(), 0);
        Files.DeleteFile(*Path);
    }
    return true;
}

/** 写入入口独立限额：不依赖上游EncodeRecord，超限不得更改已有主档或备份。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveOversizedWriteTest,
    "GamePlatform.Save.Storage.RejectsOversizedWrite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSaveOversizedWriteTest::RunTest(const FString&)
{
    IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
    const FString Folder = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("GamePlatformSaveWriteBudget"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
    TestTrue(TEXT("仅创建隔离测试文件夹"), Files.CreateDirectoryTree(*Folder));
    const FString Primary = FPaths::Combine(Folder, TEXT("source.gpsav"));
    {
        TUniquePtr<IFileHandle> Existing(Files.OpenWrite(*Primary, false, false));
        if (!Existing)
        {
            AddError(TEXT("无法建立主档保留性夹具"));
            Files.DeleteDirectory(*Folder);
            return false;
        }
        const uint8 Sentinel = 0x5A;
        if (!Existing->Write(&Sentinel, 1))
        {
            AddError(TEXT("主档夹具写入失败"));
            Existing.Reset();
            Files.DeleteFile(*Primary);
            Files.DeleteDirectory(*Folder);
            return false;
        }
    }

    TArray<uint8> Oversized;
    // 本测试分配约8MiB的独立内存夹具，不读取或修改真实用户档案。
    Oversized.SetNumZeroed(FGamePlatformSavePolicy::GetMaxEncodedBytes() + 1);
    const FGamePlatformResult Rejected = FGamePlatformSaveStorage::WriteAtomically(
        Primary, Oversized);
    TestFalse(TEXT("存储层必须拒绝超限编码数据"), Rejected.IsSuccess());
    TestEqual(TEXT("超限写入使用统一错误码"), Rejected.Code, FName(TEXT("SaveFileTooLarge")));
    TestEqual(TEXT("超限尝试不替换现有主档"), Files.FileSize(*Primary), static_cast<int64>(1));
    TestFalse(TEXT("超限尝试不生成备份"),
        Files.FileExists(*FGamePlatformSaveStorage::BuildBackupPath(Primary)));
    {
        TUniquePtr<IFileHandle> Existing(Files.OpenRead(*Primary));
        uint8 Restored = 0;
        TestTrue(TEXT("主档仍然可读"), Existing.IsValid() && Existing->Read(&Restored, 1));
        TestEqual(TEXT("原有主档字节必须原样保留"), Restored, static_cast<uint8>(0x5A));
    }
    Files.DeleteFile(*Primary);
    Files.DeleteDirectory(*Folder);
    return true;
}


/**
 * 同一进程多个GameInstance可能指向相同ProjectSavedDir：物理槽位应当跨实例独占。
 * 取消/销毁旧实例只撤销回调，线程池仍持有的租约必须延迟释放至磁盘IO实际结束。
 * 本用例使用随机纯值摘要，不创建文件、不连接后端，也不声称跨进程文件锁。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSaveProcessSlotLeaseLifecycleTest,
    "GamePlatform.Save.Storage.ProcessSlotLeaseLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveProcessSlotLeaseLifecycleTest::RunTest(const FString&)
{
    const FString StableId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString OtherId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TestFalse(TEXT("空物理槽位身份必须拒绝"),
        FGamePlatformSaveProcessSlotLease::TryAcquire(TEXT("")).IsValid());

    TSharedPtr<FGamePlatformSaveProcessSlotLease, ESPMode::ThreadSafe> Owner =
        FGamePlatformSaveProcessSlotLease::TryAcquire(StableId);
    TestTrue(TEXT("首个GameInstance获得当前存档槽位"), Owner.IsValid());
    TSharedPtr<FGamePlatformSaveProcessSlotLease, ESPMode::ThreadSafe> Worker = Owner;

    TestFalse(TEXT("另一GameInstance不能在旧IO期间占用同槽位"),
        FGamePlatformSaveProcessSlotLease::TryAcquire(StableId).IsValid());
    auto Unrelated = FGamePlatformSaveProcessSlotLease::TryAcquire(OtherId);
    TestTrue(TEXT("不同存档槽位可并行操作"), Unrelated.IsValid());

    Owner.Reset(); // 模拟GameInstance注销，只撤销主线程所有权
    TestFalse(TEXT("旧后台任务未结束前，取消回调不能提前释放进程锁"),
        FGamePlatformSaveProcessSlotLease::TryAcquire(StableId).IsValid());
    Worker.Reset(); // 模拟线程池完成最后一次文件访问

    auto NextInstance = FGamePlatformSaveProcessSlotLease::TryAcquire(StableId);
    TestTrue(TEXT("旧后台IO结束后，新GameInstance可重新取得同槽位"), NextInstance.IsValid());
    TestFalse(TEXT("旧实例与新实例的锁仍不允许重复授予"),
        FGamePlatformSaveProcessSlotLease::TryAcquire(StableId).IsValid());
    NextInstance.Reset();
    Unrelated.Reset();
    return true;
}
#endif
