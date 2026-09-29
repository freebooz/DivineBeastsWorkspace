#include "Storage/GamePlatformSaveStorage.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
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

    if (!FFileHelper::LoadFileToArray(OutBytes, *Path))
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveFileReadFailed"),
            TEXT("读取本地存档文件失败。"));
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
