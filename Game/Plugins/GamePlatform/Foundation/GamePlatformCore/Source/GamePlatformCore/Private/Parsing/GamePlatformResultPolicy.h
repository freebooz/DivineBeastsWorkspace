#pragma once

#include <string>
#include <utility>

// 原生生产策略；UE只负责FName/FString与UTF-8的边界转换，不复制诊断决策。
namespace GamePlatformCore::Private
{
enum class ResultStatus { NotExecuted, Succeeded, Failed, Cancelled, Unsupported };

struct Result
{
    ResultStatus Status = ResultStatus::NotExecuted;
    std::string Code;
    std::string Message;

    bool IsSuccess() const { return Status == ResultStatus::Succeeded && Code.empty(); }
};

/** 显式构造结果；失败缺码暴露调用错误，保留已有说明。这里的文本是UTF-8诊断，不是UI本地化。 */
inline Result MakeResult(ResultStatus Status, std::string Code, std::string Message)
{
    if (Status == ResultStatus::Succeeded) { return {Status, {}, {}}; }
    if (Status == ResultStatus::NotExecuted) { return {}; }
    if (Status == ResultStatus::Cancelled)
    {
        return {Status, "Cancelled", Message.empty() ? "操作已取消。" : std::move(Message)};
    }
    const bool bIsUnsupported = Status == ResultStatus::Unsupported;
    if (Code.empty())
    {
        Code = bIsUnsupported ? "MissingUnsupportedCode" : "MissingFailureCode";
        Message = std::string(bIsUnsupported ? "不支持结果缺少诊断代码。" : "失败结果缺少诊断代码。") + Message;
    }
    else if (Message.empty())
    {
        Message = bIsUnsupported ? "当前实现不支持所请求的能力。" : "操作失败，请根据诊断代码检查调用条件。";
    }
    return {Status, std::move(Code), std::move(Message)};
}
}
