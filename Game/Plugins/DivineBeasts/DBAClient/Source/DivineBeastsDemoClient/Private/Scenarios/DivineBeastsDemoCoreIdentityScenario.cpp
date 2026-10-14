// SSLM项目逻辑身份演示：只调用GamePlatformCore公开能力，不复制解析算法、不写账号/资源/权威状态。
#include "Scenarios/DivineBeastsDemoCoreIdentityScenario.h"
#include "Types/GamePlatformId.h"
#include "Engine/World.h"
namespace DivineBeasts::Demo
{
FGamePlatformResult EvaluateCoreIdentity(const TArray<FDivineBeastsDemoParameterValue>& Values, FString& OutCanonical)
{
    OutCanonical.Reset();
    if (Values.Num() != 3 || Values[0].Type != EDivineBeastsDemoParameterType::Identifier || Values[1].Type != EDivineBeastsDemoParameterType::Identifier ||
        Values[0].IdentifierValue.IsNone() || Values[1].IdentifierValue.IsNone() ||
        Values[2].Type != EDivineBeastsDemoParameterType::Integer || Values[2].IntegerValue < 0 || Values[2].IntegerValue > MAX_int32)
        return FGamePlatformResult::Failure(TEXT("Demo.CoreIdentityParameters"), TEXT("逻辑身份需要Namespace/Name标识和未溢出的整数版本。"));
    FGamePlatformId Created;
    if (!FGamePlatformId::TryCreate(Values[0].IdentifierValue.ToString(), Values[1].IdentifierValue.ToString(), static_cast<int32>(Values[2].IntegerValue), Created))
        return FGamePlatformResult::Failure(TEXT("Demo.CoreIdentityRejected"), TEXT("平台拒绝该身份：名称须为单段ASCII，版本须为1..INT32_MAX；输入未被夹紧。"));
    FGamePlatformId Parsed; const FString Text = Created.ToString();
    if (Text.IsEmpty() || !FGamePlatformId::TryParse(Text, Parsed) || Parsed != Created || GetTypeHash(Parsed) != GetTypeHash(Created))
        return FGamePlatformResult::Failure(TEXT("Demo.CoreIdentityRoundTripMismatch"), TEXT("平台创建、规范文本、解析、相等及容器哈希未匹配。"));
    OutCanonical = Text; auto Result = FGamePlatformResult::Success();
    Result.Message = TEXT("真实平台逻辑身份回环一致：") + Text; return Result;
}
/** 本次只拥有活动标记及弱世界；结束不操作其他实例或任何持久值。 */
class FCoreIdentityScenario final : public IDivineBeastsDemoScenarioAdapter
{
public:
    void Prepare(const FDivineBeastsDemoScenarioContext& Input, FDivineBeastsDemoOperationCompletion Completion) override
    {
        Context = Input;
        // 参数位置是本适配的明确合同，不能把其他三个合法字段悄悄解释成身份字段。
        const auto& Parameters = Context.Scenario.Parameters;
        bPrepared = Context.Token.IsValid() && Context.World.IsValid() && !Context.World->bIsTearingDown &&
            Context.Scenario.AdapterId == TEXT("CoreIdentity") && Parameters.Num() == 3 &&
            Parameters[0].ParameterId == TEXT("Namespace") && Parameters[1].ParameterId == TEXT("Name") && Parameters[2].ParameterId == TEXT("LogicalVersion");
        Completion(Context.Token, bPrepared ? FGamePlatformResult::Success() : FGamePlatformResult::Failure(TEXT("Demo.CoreIdentityWorldMissing"), TEXT("缺本次有效Token/世界。")));
    }
    void Execute(FName Operation, const TArray<FDivineBeastsDemoParameterValue>& Values, FDivineBeastsDemoOperationCompletion Completion) override
    {
        if (!bPrepared || !Context.World.IsValid() || Context.World->bIsTearingDown)
        { Completion(Context.Token, FGamePlatformResult::Failure(TEXT("Demo.CoreIdentityInactive"), TEXT("本次身份样板已停止或世界失效。"))); return; }
        if (Operation != TEXT("Execute"))
        { Completion(Context.Token, FGamePlatformResult::Unsupported(TEXT("Demo.CoreIdentityOperation"), TEXT("身份值演示只支持Execute及Stop，无需复位持久业务。"))); return; }
        FString Canonical; Completion(Context.Token, EvaluateCoreIdentity(Values, Canonical));
    }
    void Stop(FDivineBeastsDemoOperationCompletion Completion) override
    {
        // 没有外部资源或异步请求；失活当前实例即完成自己的清理，重复Stop不触碰其他实例。
        bPrepared = false; Completion(Context.Token, FGamePlatformResult::Success());
    }
private:
    FDivineBeastsDemoScenarioContext Context;
    bool bPrepared = false;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateCoreIdentityScenario() { return MakeUnique<FCoreIdentityScenario>(); }
}
