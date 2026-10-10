// 平台本地存档纯值策略：双端无关客户端Envelope校验；不访问UObject/磁盘/网络，线程间值传递而无共享可变状态。
#include "Policy/GamePlatformSavePolicy.h"

#include "Misc/Crc.h"
#include "Misc/SecureHash.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Containers/StringConv.h"

namespace
{
    constexpr uint32 SaveEnvelopeMagic = 0x47505356; // "GPSV"
    constexpr int32 SaveEnvelopeFormatVersion = 1;
    constexpr int32 MaxPayloadBytes = 8 * 1024 * 1024;
    constexpr int32 MaxNamespaceChars = 64;
    constexpr int32 MaxProfileKeyChars = 128;
    constexpr int32 MaxSlotNameChars = 64;
    constexpr int32 FixedHeaderBytes =
        sizeof(uint32) + sizeof(int32) + sizeof(int32) +
        sizeof(int32) + sizeof(uint32);

    bool IsTextComponentValid(
        const FString& Value,
        const int32 MaxChars)
    {
        if (Value.IsEmpty() || Value.Len() > MaxChars)
        {
            return false;
        }

        // 原始Key只参与摘要而不进入路径，但仍拒绝控制字符，避免诊断、日志或未来扩展产生歧义。
        for (const TCHAR Character : Value)
        {
            if (Character < TEXT(' '))
            {
                return false;
            }
        }
        return true;
    }
}

int32 FGamePlatformSavePolicy::GetMaxEncodedBytes()
{
    return FixedHeaderBytes + MaxPayloadBytes;
}

int32 FGamePlatformSavePolicy::GetMaxPayloadBytes()
{
    return MaxPayloadBytes;
}

FGamePlatformResult FGamePlatformSavePolicy::ValidateKey(
    const FGamePlatformSaveKey& Key)
{
    const FString NamespaceText = Key.Namespace.ToString();
    if (Key.Namespace.IsNone() ||
        !IsTextComponentValid(NamespaceText, MaxNamespaceChars))
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveNamespaceInvalid"),
            TEXT("本地存档Namespace不能为空且长度必须位于平台安全范围内。"));
    }

    if (!IsTextComponentValid(Key.ProfileKey, MaxProfileKeyChars))
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveProfileKeyInvalid"),
            TEXT("本地存档ProfileKey不能为空且长度必须位于平台安全范围内。"));
    }

    if (!IsTextComponentValid(Key.SlotName, MaxSlotNameChars))
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveSlotNameInvalid"),
            TEXT("本地存档SlotName不能为空且长度必须位于平台安全范围内。"));
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult FGamePlatformSavePolicy::ValidateRecord(
    const FGamePlatformSaveRecord& Record)
{
    const FGamePlatformResult KeyResult = ValidateKey(Record.Key);
    if (!KeyResult.IsSuccess())
    {
        return KeyResult;
    }

    if (Record.SchemaVersion <= 0)
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveSchemaVersionInvalid"),
            TEXT("本地存档SchemaVersion必须为正整数。"));
    }

    if (Record.Payload.Num() > MaxPayloadBytes)
    {
        return FGamePlatformResult::Failure(
            TEXT("SavePayloadTooLarge"),
            TEXT("本地存档Payload超过平台单记录安全上限。"));
    }

    return FGamePlatformResult::Success();
}

FString FGamePlatformSavePolicy::BuildStableStorageId(
    const FGamePlatformSaveKey& Key)
{
    // Key从不直接进入路径；稳定摘要同时避免路径穿越和原始ProfileKey暴露在文件名。
    const FString Material = FString::Printf(
        TEXT("%s\n%s\n%s"),
        *Key.Namespace.ToString(),
        *Key.ProfileKey,
        *Key.SlotName);

    const FTCHARToUTF8 Utf8(*Material);
    uint8 Hash[20] = {};
    FSHA1::HashBuffer(
        Utf8.Get(),
        static_cast<uint32>(Utf8.Length()),
        Hash);

    FString Result;
    Result.Reserve(40);
    for (const uint8 Byte : Hash)
    {
        Result += FString::Printf(TEXT("%02x"), Byte);
    }
    return Result;
}

FGamePlatformResult FGamePlatformSavePolicy::EncodeRecord(
    const FGamePlatformSaveRecord& Record,
    TArray<uint8>& OutEncoded)
{
    const FGamePlatformResult Validation = ValidateRecord(Record);
    if (!Validation.IsSuccess())
    {
        return Validation;
    }

    OutEncoded.Reset();
    OutEncoded.Reserve(FixedHeaderBytes + Record.Payload.Num());

    uint32 Magic = SaveEnvelopeMagic;
    int32 FormatVersion = SaveEnvelopeFormatVersion;
    int32 SchemaVersion = Record.SchemaVersion;
    int32 PayloadSize = Record.Payload.Num();
    uint32 PayloadCrc = PayloadSize > 0
        ? FCrc::MemCrc32(Record.Payload.GetData(), PayloadSize)
        : 0u;

    FMemoryWriter Writer(OutEncoded, true);
    Writer << Magic;
    Writer << FormatVersion;
    Writer << SchemaVersion;
    Writer << PayloadSize;
    Writer << PayloadCrc;

    if (PayloadSize > 0)
    {
        Writer.Serialize(
            const_cast<uint8*>(Record.Payload.GetData()),
            PayloadSize);
    }

    if (Writer.IsError())
    {
        OutEncoded.Reset();
        return FGamePlatformResult::Failure(
            TEXT("SaveEnvelopeEncodeFailed"),
            TEXT("本地存档Envelope编码失败。"));
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult FGamePlatformSavePolicy::DecodeRecord(
    const FGamePlatformSaveKey& Key,
    const TArray<uint8>& Encoded,
    FGamePlatformSaveRecord& OutRecord)
{
    const FGamePlatformResult KeyValidation = ValidateKey(Key);
    if (!KeyValidation.IsSuccess())
    {
        return KeyValidation;
    }

    if (Encoded.Num() < FixedHeaderBytes ||
        Encoded.Num() > FixedHeaderBytes + MaxPayloadBytes)
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveEnvelopeSizeInvalid"),
            TEXT("本地存档Envelope长度非法。"));
    }

    // FMemoryReader需要可变TArray引用，因此复制输入；解码在线程池执行，不影响游戏线程热路径。
    TArray<uint8> Buffer = Encoded;
    FMemoryReader Reader(Buffer, true);

    uint32 Magic = 0;
    int32 FormatVersion = 0;
    int32 SchemaVersion = 0;
    int32 PayloadSize = 0;
    uint32 StoredCrc = 0;

    Reader << Magic;
    Reader << FormatVersion;
    Reader << SchemaVersion;
    Reader << PayloadSize;
    Reader << StoredCrc;

    if (Reader.IsError() ||
        Magic != SaveEnvelopeMagic ||
        FormatVersion != SaveEnvelopeFormatVersion)
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveEnvelopeFormatUnsupported"),
            TEXT("本地存档Envelope格式无效或版本不受支持。"));
    }

    if (SchemaVersion <= 0)
    {
        return FGamePlatformResult::Failure(
            TEXT("SaveSchemaVersionInvalid"),
            TEXT("本地存档SchemaVersion非法。"));
    }

    if (PayloadSize < 0 ||
        PayloadSize > MaxPayloadBytes ||
        Reader.TotalSize() - Reader.Tell() != PayloadSize)
    {
        return FGamePlatformResult::Failure(
            TEXT("SavePayloadSizeInvalid"),
            TEXT("本地存档Payload长度字段与实际数据不一致。"));
    }

    TArray<uint8> Payload;
    Payload.SetNumUninitialized(PayloadSize);
    if (PayloadSize > 0)
    {
        Reader.Serialize(Payload.GetData(), PayloadSize);
    }

    if (Reader.IsError())
    {
        return FGamePlatformResult::Failure(
            TEXT("SavePayloadReadFailed"),
            TEXT("本地存档Payload读取失败。"));
    }

    const uint32 ActualCrc = PayloadSize > 0
        ? FCrc::MemCrc32(Payload.GetData(), PayloadSize)
        : 0u;
    if (ActualCrc != StoredCrc)
    {
        return FGamePlatformResult::Failure(
            TEXT("SavePayloadCorrupted"),
            TEXT("本地存档CRC校验失败，数据可能损坏。"));
    }

    OutRecord = FGamePlatformSaveRecord();
    OutRecord.Key = Key;
    OutRecord.SchemaVersion = SchemaVersion;
    OutRecord.Payload = MoveTemp(Payload);
    return FGamePlatformResult::Success();
}
