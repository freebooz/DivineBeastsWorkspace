#pragma once

// GamePlatformSave纯值策略：输入校验、稳定文件摘要与版本化Envelope编解码。
// 不访问UObject、磁盘、网络或项目层状态，因此可安全在线程池中使用。
#include "Types/GamePlatformSaveTypes.h"

class FGamePlatformSavePolicy final
{
public:
    /** 单条本地记录安全上限；用于抵御损坏长度和误用，不代表网络或服务器数据预算。 */
    static int32 GetMaxPayloadBytes();
    /** 存储读取预算：最大载荷加固定Envelope头；读取前即拒绝超限。 */
    static int32 GetMaxEncodedBytes();

    /** 严格校验逻辑Key。 */
    static FGamePlatformResult ValidateKey(
        const FGamePlatformSaveKey& Key);

    /** 严格校验完整记录，包括SchemaVersion与Payload上限。 */
    static FGamePlatformResult ValidateRecord(
        const FGamePlatformSaveRecord& Record);

    /** 对逻辑Key生成稳定40字符SHA-1摘要；仅用于文件身份，不作为安全签名。 */
    static FString BuildStableStorageId(
        const FGamePlatformSaveKey& Key);

    /** 编码插件内部Envelope；包含Magic、FormatVersion、SchemaVersion、长度和CRC32。 */
    static FGamePlatformResult EncodeRecord(
        const FGamePlatformSaveRecord& Record,
        TArray<uint8>& OutEncoded);

    /** 解码并验证Envelope；CRC不匹配、未知格式或非法长度均Fail Closed。 */
    static FGamePlatformResult DecodeRecord(
        const FGamePlatformSaveKey& Key,
        const TArray<uint8>& Encoded,
        FGamePlatformSaveRecord& OutRecord);
};
