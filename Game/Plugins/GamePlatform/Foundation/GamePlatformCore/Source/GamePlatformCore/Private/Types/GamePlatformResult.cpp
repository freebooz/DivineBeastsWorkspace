#include "Types/GamePlatformResult.h"
#include "Containers/StringConv.h"
#include "Parsing/GamePlatformResultPolicy.h"

namespace
{
using FNativeResult = GamePlatformCore::Private::Result;
using ENativeStatus = GamePlatformCore::Private::ResultStatus;

std::string ToUtf8(const FString& Text)
{
    // 显式长度保留诊断中的内嵌NUL；转换临时值不逃逸出本函数。
    const FTCHARToUTF8 Converted(*Text, Text.Len());
    return std::string(Converted.Get(), Converted.Length());
}

FString FromUtf8(const std::string& Text)
{
    const FUTF8ToTCHAR Converted(Text.data(), static_cast<int32>(Text.size()));
    return FString(Converted.Length(), Converted.Get());
}

ENativeStatus ToNativeStatus(EGamePlatformResultStatus Status)
{
    // 逐项映射，不依赖两个独立枚举偶然相同的整数值。
    switch (Status)
    {
    case EGamePlatformResultStatus::Succeeded: return ENativeStatus::Succeeded;
    case EGamePlatformResultStatus::Failed: return ENativeStatus::Failed;
    case EGamePlatformResultStatus::Cancelled: return ENativeStatus::Cancelled;
    case EGamePlatformResultStatus::Unsupported: return ENativeStatus::Unsupported;
    default: return ENativeStatus::NotExecuted;
    }
}

FGamePlatformResult BuildResult(EGamePlatformResultStatus Status, FName Code, const FString& Message)
{
    // FName的None是语义空值，其ToString()会得到"None"，必须在进入策略前转为空串。
    const FNativeResult Native = GamePlatformCore::Private::MakeResult(
        ToNativeStatus(Status), Code.IsNone() ? std::string{} : ToUtf8(Code.ToString()), ToUtf8(Message));
    FGamePlatformResult Result;
    Result.Status = Status;
    Result.Code = Native.Code.empty() ? NAME_None : FName(*FromUtf8(Native.Code));
    Result.Message = FromUtf8(Native.Message);
    return Result;
}
}

FGamePlatformResult FGamePlatformResult::Success()
{
    return BuildResult(EGamePlatformResultStatus::Succeeded, NAME_None, FString{});
}

FGamePlatformResult FGamePlatformResult::Failure(FName Code, FString Message)
{
    return BuildResult(EGamePlatformResultStatus::Failed, Code, Message);
}

FGamePlatformResult FGamePlatformResult::Cancelled(FString Message)
{
    return BuildResult(EGamePlatformResultStatus::Cancelled, NAME_None, Message);
}

FGamePlatformResult FGamePlatformResult::Unsupported(FName Code, FString Message)
{
    return BuildResult(EGamePlatformResultStatus::Unsupported, Code, Message);
}

bool FGamePlatformResult::IsSuccess() const
{
    // 不把非法枚举、默认值或附带错误码的手工结果解释为成功。
    return FNativeResult{ToNativeStatus(Status), Code.IsNone() ? std::string{} : "Present", {}}.IsSuccess();
}
