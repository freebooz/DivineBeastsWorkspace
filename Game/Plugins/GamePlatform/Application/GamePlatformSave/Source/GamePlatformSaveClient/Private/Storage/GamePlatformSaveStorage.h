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

    /** 有界读取单个主档或备份；不存在返回SaveFileNotFound，超限返回SaveFileTooLarge且不分配载荷，读取/长度变化失败清空OutBytes。 */
    static FGamePlatformResult ReadFile(
        const FString& Path,
        TArray<uint8>& OutBytes);

    /**
     * 同目录临时文件写入并Full Flush，然后使用IFileManager::Move替换主档。
     * 存储层独立检查编码后的字节限额；超限先拒绝，不创建目录、临时文件或备份。
     * 若旧主档存在，替换前先保存一份.bak；备份失败清理本次临时文件，
     * 不覆盖原主档及旧备份（禁止把已有文件当成可清理的失败夹具）。
     */
    static FGamePlatformResult WriteAtomically(
        const FString& PrimaryPath,
        const TArray<uint8>& Encoded);

    /** 幂等删除主档和.bak；不存在视为成功。 */
    static FGamePlatformResult DeleteRecordFiles(
        const FString& PrimaryPath);
};
