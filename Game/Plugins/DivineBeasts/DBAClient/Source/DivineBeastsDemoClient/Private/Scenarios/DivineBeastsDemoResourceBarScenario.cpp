// SSLM开发UI资源条样板；只修改指定样板控件，保存/恢复本次展示快照，不接玩家GAS。
#include "Scenarios/DivineBeastsDemoSampleAdapters.h"
#include "Components/GamePlatformResourceBarWidget.h"
#include "Engine/World.h"
namespace DivineBeasts::Demo
{
/** 活动实例只弱持指定World/Widget；Stop恢复原值，失效对象明确报告失败。 */
class FResourceBarScenario final : public IDivineBeastsDemoScenarioAdapter
{
public:
    ~FResourceBarScenario() override
    {
        // 仅恢复仍存在的指定样板控件；析构清理没有回执，不覆盖已发布失败或取消结果。
        if (bHasOriginal && Context.World.IsValid() && !Context.World->bIsTearingDown && Widget.IsValid()) Widget->ApplyResourceState(Original);
    }
    void Prepare(const FDivineBeastsDemoScenarioContext& Input, FDivineBeastsDemoOperationCompletion Completion) override
    {
        Context = Input; Widget = Cast<UGamePlatformResourceBarWidget>(Input.SampleWidget.Get());
        if (!Context.World.IsValid() || !Widget.IsValid() || Widget->GetWorld() != Context.World.Get())
        { Completion(Context.Token, FGamePlatformResult::Failure(FName(TEXT("Demo.ResourceBarMissing")), TEXT("指定世界没有已加载的真实资源条样板控件。"))); return; }
        Original = Widget->GetResourceState(); bHasOriginal = true; Completion(Context.Token, FGamePlatformResult::Success());
    }
    void Execute(FName Operation, const TArray<FDivineBeastsDemoParameterValue>& Values, FDivineBeastsDemoOperationCompletion Completion) override
    {
        if (!bHasOriginal || !Widget.IsValid() || !Context.World.IsValid())
        { Completion(Context.Token, FGamePlatformResult::Failure(FName(TEXT("Demo.ResourceBarInvalid")), TEXT("样板控件或世界已失效。"))); return; }
        if (Operation == TEXT("Reset")) { Widget->ApplyResourceState(Original); Completion(Context.Token, FGamePlatformResult::Success()); return; }
        if (Operation != TEXT("Execute") || Values.Num() != Context.Scenario.Parameters.Num())
        { Completion(Context.Token, FGamePlatformResult::Unsupported(FName(TEXT("Demo.ResourceBarOperationUnsupported")), TEXT("资源条仅支持Execute/Reset。"))); return; }
        FGamePlatformUIResourceBarState State;
        State.ResourceId = TEXT("DemoResource"); State.bShowValueText = true;
        bool bCurrent = false, bMaximum = false;
        for (int32 Index = 0; Index < Values.Num(); ++Index)
        {
            if (Values[Index].Type != EDivineBeastsDemoParameterType::Number) continue;
            if (Context.Scenario.Parameters[Index].ParameterId == TEXT("Current")) { State.CurrentValue = Values[Index].NumberValue; bCurrent = true; }
            if (Context.Scenario.Parameters[Index].ParameterId == TEXT("Maximum")) { State.MaximumValue = Values[Index].NumberValue; bMaximum = true; }
        }
        if (!bCurrent || !bMaximum || !FMath::IsFinite(State.CurrentValue) || !FMath::IsFinite(State.MaximumValue))
        { Completion(Context.Token, FGamePlatformResult::Failure(FName(TEXT("Demo.ResourceBarParametersMissing")), TEXT("资源条需有限Current/Maximum参数。"))); return; }
        Widget->ApplyResourceState(State);
        const auto Actual = Widget->GetResourceState();
        Completion(Context.Token, Actual.CurrentValue == State.CurrentValue && Actual.MaximumValue == State.MaximumValue
            ? FGamePlatformResult::Success() : FGamePlatformResult::Failure(FName(TEXT("Demo.ResourceBarStateMismatch")), TEXT("真实资源条状态未匹配提交值。")));
    }
    void Stop(FDivineBeastsDemoOperationCompletion Completion) override
    {
        if (!bHasOriginal) { Completion(Context.Token, FGamePlatformResult::Success()); return; }
        if (!Widget.IsValid() || !Context.World.IsValid())
        { bHasOriginal = false; Completion(Context.Token, FGamePlatformResult::Failure(FName(TEXT("Demo.ResourceBarRestoreFailed")), TEXT("样板已失效，未确认展示状态恢复。"))); return; }
        Widget->ApplyResourceState(Original); bHasOriginal = false; Completion(Context.Token, FGamePlatformResult::Success());
    }
private:
    FDivineBeastsDemoScenarioContext Context;
    TWeakObjectPtr<UGamePlatformResourceBarWidget> Widget;
    FGamePlatformUIResourceBarState Original;
    bool bHasOriginal = false;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateResourceBarScenario() { return MakeUnique<FResourceBarScenario>(); }
}
