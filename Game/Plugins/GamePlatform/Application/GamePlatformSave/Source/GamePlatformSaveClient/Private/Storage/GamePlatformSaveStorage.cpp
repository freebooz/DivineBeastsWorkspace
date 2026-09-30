// 平台客户端本地存储：调用方提供路径与已编码数据，后台线程只操作自有句柄，主档和备份统一限长，失败不伪造成功或业务回滚。
#include "Storage/GamePlatformSaveStorage.h"
#include "Policy/GamePlatformSavePolicy.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

namespace
{
    FGamePlatformResult WriteTempFile(
        const FString& TempPath,
        const TArray<uint8>& Encoded)
    {
        IPlatformFile& PlatformFile =
            FPlatformFileManager::Get().GetPlatformFile();

        TUniquePtr<IFileHandle> Handle(
            PlatformFile.OpenWrite(*TempPath, false, false));
        if (!Handle)
        {
            return FGamePlatformResult::Failure(
                TEXT("SaveTempOpenFailed"),
                TEXT("无法创建本地存档临时文件。"));
        }

        if (Encoded.Num() > 0 &&
            !Handle->Write(Encoded.GetData(), Encoded.Num()))
        {
            Handle.Reset();
            PlatformFile.DeleteFile(*TempPath);
            return FGamePlatformResult::Failure(
                TEXT("SaveTempWriteFailed"),
                TEXT("写入本地存档临时文件失败。"));
        }

        // Full Flush失败时必须Fail Closed，不能把尚未可靠刷新的临时文件替换为正式主档。
        if (!Handle->Flush(true))
        {
            Handle.Reset();
            PlatformFile.DeleteFile(*TempPath);
            return FGamePlatformResult::Failure(
                TEXT("SaveTempFlushFailed"),
                TEXT("完整刷新本地存档临时文件失败，未替换正式主档。"));
        }

        Handle.Reset();
        return FGamePlatformResult::Success();
    }

    bool ReplaceByMove(
        const FString& Destination,
        const FString& Source)
    {
        return IFileManager::Get().Move(
            *Destination,
            *Source,
            true,
            true,
            false,
            true);
    }
}

FString FGamePlatformSaveStorage::BuildPrimaryPath(
    const FString& StableStorageId)
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("GamePlatformSave"),
        StableStorageId + TEXT(".gpsav"));
}

FString FGamePlatformSaveStorage::BuildBackupPath(
    const FString& PrimaryPath)
{
    return PrimaryPath + TEXT(".bak");
}

FGamePlatformResult FGamePlatformSaveStorage::ReadFile(
    const FString& Path,
    TArray<uint8>& OutBytes)
{
    OutBytes.Reset();

    IPlatformFile& PlatformFile =
        FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.FileExists(*Path))
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveFileNotFound"),
            TEXT("本地存档文件不存在。"));
    }

    // 主档和备份共用此预算。先检查目录元数据，再检查已打开句柄，避免检查/打开间文件增长。
    const int64 MaxBytes = FGamePlatformSavePolicy::GetMaxEncodedBytes();
    const int64 AdvertisedBytes = PlatformFile.FileSize(*Path);
    if (AdvertisedBytes > MaxBytes)
    {
        return FGamePlatformResult::Failure(TEXT("SaveFileTooLarge"), TEXT("本地存档超出Envelope读取预算，未分配载荷缓冲。"));
    }
    TUniquePtr<IFileHandle> Handle(PlatformFile.OpenRead(*Path));
    if (!Handle)
    {
        return FGamePlatformResult::Failure(TEXT("SaveFileReadFailed"), TEXT("无法打开本地存档文件。"));
    }
    const int64 ReadBytes = Handle->Size();
    if (ReadBytes > MaxBytes)
    {
        return FGamePlatformResult::Failure(TEXT("SaveFileTooLarge"), TEXT("打开的本地存档超出Envelope读取预算。"));
    }
    if (ReadBytes < 0)
    {
        return FGamePlatformResult::Failure(TEXT("SaveFileReadFailed"), TEXT("无法确定本地存档长度。"));
    }
    // 固定长度读取从不追随文件增长；失败或长度变化时清除部分载荷，交给调用方明确回退备份。
    OutBytes.SetNumUninitialized(static_cast<int32>(ReadBytes));
    if ((ReadBytes > 0 && !Handle->Read(OutBytes.GetData(), ReadBytes)) || Handle->Size() != ReadBytes)
    {
        OutBytes.Reset();
        return FGamePlatformResult::Failure(TEXT("SaveFileReadFailed"), TEXT("读取存档失败或读取期间长度改变。"));
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult FGamePlatformSaveStorage::WriteAtomically(
    const FString& PrimaryPath,
    const TArray<uint8>& Encoded)
{
    IPlatformFile& PlatformFile =
        FPlatformFileManager::Get().GetPlatformFile();

    const FString Directory = FPaths::GetPath(PrimaryPath);
    if (!PlatformFile.CreateDirectoryTree(*Directory))
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveDirectoryCreateFailed"),
            TEXT("无法创建本地存档目录。"));
    }

    const FString TempPath = PrimaryPath + TEXT(".tmp.") +
        FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FGamePlatformResult Result = WriteTempFile(TempPath, Encoded);
    if (!Result.IsSuccess())
    {
        return Result;
    }

    const FString BackupPath = BuildBackupPath(PrimaryPath);
    if (PlatformFile.FileExists(*PrimaryPath))
    {
        const FString BackupTempPath = BackupPath + TEXT(".tmp.") +
            FGuid::NewGuid().ToString(EGuidFormats::Digits);

        if (!PlatformFile.CopyFile(*BackupTempPath, *PrimaryPath))
        {
            PlatformFile.DeleteFile(*TempPath);
            return FGamePlatformResult::Failure(
                TEXT("SaveBackupCopyFailed"),
                TEXT("创建本地存档备份失败，已保留原主档。"));
        }

        if (!ReplaceByMove(BackupPath, BackupTempPath))
        {
            PlatformFile.DeleteFile(*BackupTempPath);
            PlatformFile.DeleteFile(*TempPath);
            return FGamePlatformResult::Failure(
                TEXT("SaveBackupReplaceFailed"),
                TEXT("替换本地存档备份失败，已保留原主档。"));
        }
    }

    // Temp与Primary位于同一目录，避免跨卷移动破坏替换语义。
    if (!ReplaceByMove(PrimaryPath, TempPath))
    {
        PlatformFile.DeleteFile(*TempPath);
        return FGamePlatformResult::Failure(
            TEXT("SavePrimaryReplaceFailed"),
            TEXT("原子替换本地存档主文件失败。"));
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult FGamePlatformSaveStorage::DeleteRecordFiles(
    const FString& PrimaryPath)
{
    IFileManager& FileManager = IFileManager::Get();
    const FString BackupPath = BuildBackupPath(PrimaryPath);

    bool bOk = true;
    if (FileManager.FileExists(*PrimaryPath))
    {
        bOk = FileManager.Delete(
            *PrimaryPath,
            false,
            true,
            true) && bOk;
    }

    if (FileManager.FileExists(*BackupPath))
    {
        bOk = FileManager.Delete(
            *BackupPath,
            false,
            true,
            true) && bOk;
    }

    if (!bOk)
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveDeleteFailed"),
            TEXT("删除本地存档主文件或备份失败。"));
    }

    return FGamePlatformResult::Success();
}
