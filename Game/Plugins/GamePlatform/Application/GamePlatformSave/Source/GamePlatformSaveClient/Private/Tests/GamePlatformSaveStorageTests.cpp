// 存档存储回归：本地临时夹具验证主档/备份超限在读取前拒绝，不测生产用户数据。
#include "Storage/GamePlatformSaveStorage.h"
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
#endif
