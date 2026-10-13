// SSLM鸟群样板组合正式公开操作，不另建Niagara执行器；请求受理与GPU画面分开。
#include "Scenarios/DivineBeastsDemoSampleAdapters.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Engine/World.h"
namespace DivineBeasts::Demo
{
/** 准备前存在旧兼容修改时拒绝接管，避免复位别人的状态。 */
class FBirdFlockScenario final : public IDivineBeastsDemoScenarioAdapter
{
public:
    ~FBirdFlockScenario() override
    {
        // 世界仍有效而适配因注销/超时析构时，尽力恢复自己的记录；世界退出不解引用旧Actor。
        // 析构没有成功回执，不能把已有Cleanup失败改判成功。
        if (bNeedsRestore && Context.World.IsValid() && !Context.World->bIsTearingDown && Controller.IsValid()) Controller->RestoreBirdEdits();
    }
    void Prepare(const FDivineBeastsDemoScenarioContext& Input, FDivineBeastsDemoOperationCompletion Completion) override
    {
        Context = Input; Controller = Context.World.IsValid() ? Context.World->GetSubsystem<UDivineBeastsDemoWorldSubsystem>() : nullptr;
        if (!Controller.IsValid() || Controller->GetBirdActorCount() == 0 || Controller->HasPendingLegacyEdits())
        { Completion(Context.Token, FGamePlatformResult::Failure(FName(TEXT("Demo.BirdSceneUnavailable")), TEXT("缺正式鸟群组件，或旧兼容操作尚未复位，拒绝接管。"))); return; }
        bPrepared = true; Completion(Context.Token, FGamePlatformResult::Success());
    }
    void Execute(FName Operation, const TArray<FDivineBeastsDemoParameterValue>& Values, FDivineBeastsDemoOperationCompletion Completion) override
    {
        if (!bPrepared || !Controller.IsValid() || !Context.World.IsValid())
        { Completion(Context.Token, FGamePlatformResult::Failure(FName(TEXT("Demo.BirdWorldInvalid")), TEXT("鸟群样板世界或控制器失效。"))); return; }
        if (Operation == TEXT("Reset"))
        {
            const bool bRestored = !bNeedsRestore || Controller->RestoreBirdEdits();
            if (bRestored) bNeedsRestore = false;
            Completion(Context.Token, bRestored ? FGamePlatformResult::Success() : FGamePlatformResult::Failure(FName(TEXT("Demo.BirdRestoreFailed")), Controller->GetLastFeedback().ToString())); return;
        }
        if (Operation != TEXT("Execute") || Values.Num() != 1 || Values[0].Type != EDivineBeastsDemoParameterType::Integer || Values[0].IntegerValue < 0 || Values[0].IntegerValue > 512)
        { Completion(Context.Token, FGamePlatformResult::Failure(FName(TEXT("Demo.BirdParameterInvalid")), TEXT("鸟群Count需整数0..512，范围来自正式SceneComponent合同。"))); return; }
        bNeedsRestore = true; // 失败也可能产生本次原值记录，清理仍须撤销这份所有权。
        const bool bAccepted = Controller->ApplyBirdCount(static_cast<int32>(Values[0].IntegerValue));
        Completion(Context.Token, bAccepted ? FGamePlatformResult::Success() : FGamePlatformResult::Failure(FName(TEXT("Demo.BirdRequestRejected")), Controller->GetLastFeedback().ToString()));
    }
    void Stop(FDivineBeastsDemoOperationCompletion Completion) override
    {
        const bool bRestored = !bNeedsRestore || (Context.World.IsValid() && Controller.IsValid() && Controller->RestoreBirdEdits());
        bPrepared = false; if (bRestored) bNeedsRestore = false;
        Completion(Context.Token, bRestored ? FGamePlatformResult::Success() : FGamePlatformResult::Failure(FName(TEXT("Demo.BirdRestoreFailed")), TEXT("鸟群原状态未确认恢复，拒绝正常返程。")));
    }
private:
    FDivineBeastsDemoScenarioContext Context;
    TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> Controller;
    bool bPrepared = false;
    bool bNeedsRestore = false;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateBirdFlockScenario() { return MakeUnique<FBirdFlockScenario>(); }
}
