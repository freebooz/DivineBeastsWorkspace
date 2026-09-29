#pragma once

// GamePlatformSave文件存储适配。
// 只接收稳定摘要和已经编码的字节，不理解业务Schema，也不接触UObject。
#include "CoreMinimal.h"
#include "Types/GamePlatformResult.h"

class FGamePlatformSaveStorage final
{
public:
    /** 根据稳定摘要生成主档绝对路径。 */
    static FString BuildPrimaryPath(const FString& StableStorageId);

    /** 生成主档对应的.bak备份路径。 */
    static FString BuildBackupPath(const FString& PrimaryPath);

    /** 读取单个文件；不存在返回SaveFileNotFound。 */
    static FGamePlatformResult ReadFile(
        const FString& Path,
        TArray<uint8>& OutBytes);

    /**
     * 同目录临时文件写入并Full Flush，然后使用IFileManager::Move替换主档。
     * 若旧主档存在，替换前先保存一份.bak；备份步骤失败则不覆盖当前主档。
     */
    static FGamePlatformResult WriteAtomically(
        const FString& PrimaryPath,
        const TArray<uint8>& Encoded);

    /** 幂等删除主档和.bak；不存在视为成功。 */
    static FGamePlatformResult DeleteRecordFiles(
        const FString& PrimaryPath);
};
