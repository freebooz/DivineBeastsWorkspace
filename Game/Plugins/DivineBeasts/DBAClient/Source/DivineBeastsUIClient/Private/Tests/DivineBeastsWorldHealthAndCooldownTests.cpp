// 项目UI真实资源事件回归：只验证隔离实例的UMG投影、清理及冷却材质参数，不连接后端。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/GamePlatformSlotWidget.h"
#include "WorldUI/GamePlatformWorldNameplateWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/StrongObjectPtr.h"
#include "Blueprint/WidgetLayoutLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldHealthResourceTest,
    "DivineBeasts.UI.World.HealthBar.ResourceEventProjection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsWorldHealthResourceTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto* Class = LoadClass<UGamePlatformWorldNameplateWidget>(nullptr,
        TEXT("/DBAUIPack_Core/UI/World/WBP_DBA_WorldHealthBar.WBP_DBA_WorldHealthBar_C"));
    if (!TestNotNull(TEXT("真实头顶血条类"), Class)) return false;
    TStrongObjectPtr<UGamePlatformWorldNameplateWidget> Widget(CreateWidget<UGamePlatformWorldNameplateWidget>(Instance.Get(), Class));
    if (!TestNotNull(TEXT("真实名称板实例"), Widget.Get())) return false;
    auto* Fill = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("ResourceFill")));
    if (!TestNotNull(TEXT("真实命名生命填充"), Fill)) return false;
    FGamePlatformWorldUIRequest Request; Request.RequestId = FGuid::NewGuid(); Request.SurfaceKey = TEXT("HealthTest");
    Request.Resource.MaximumValue = 300.; Request.Resource.CurrentValue = 300.;
    Widget->ApplyWorldUIRequest(Request); TestEqual(TEXT("初始化事件显示真实满血"), Fill->GetPercent(), 1.f);
    Request.Resource.CurrentValue = 90.; Widget->ApplyWorldUIRequest(Request);
    TestEqual(TEXT("伤害事件立即更新30%血条，不等待Tick"), Fill->GetPercent(), .3f);
    Request.Resource.MaximumValue = 0.; Widget->ApplyWorldUIRequest(Request);
    TestEqual(TEXT("无合法上限不能伪装满血"), Fill->GetPercent(), 0.f);
    // 无Ticker时首次事件必须完成排版，不能等镜头移动才修正零尺寸偏移。
    Widget->TakeWidget();
    Widget->SetProjectedState(FVector2D(500, 300), true, 100.f);
    TestTrue(TEXT("静止镜头首投影获得真实160x44尺寸"), Widget->GetDesiredSize().Equals(FVector2D(160, 44), .01));
    const float Scale = UWidgetLayoutLibrary::GetViewportScale(Widget.Get());
    TestTrue(TEXT("首投影按DPI居中并置于目标上方"), Widget->GetRenderTransform().Translation.Equals(
        FVector2D(500, 300) / FMath::Max(Scale, .001f) - FVector2D(80, 44), .01));
    Widget->SetProjectedState(FVector2D::ZeroVector, false, 100.f);
    Widget->SetProjectedState(FVector2D(700, 400), true, 100.f);
    TestTrue(TEXT("隐藏后重显立即重新排版定位"), Widget->GetRenderTransform().Translation.Equals(
        FVector2D(700, 400) / FMath::Max(Scale, .001f) - FVector2D(80, 44), .01));
    Widget->ResetWorldUIState(); TestFalse(TEXT("回收释放旧显示注册身份"), Widget->GetWorldUIRequest().RequestId.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCooldownSweepEventTest,
    "DivineBeasts.UI.Combat.CooldownSweep.EventParametersAndClear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCooldownSweepEventTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString())); UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto* Class = LoadClass<UGamePlatformSlotWidget>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilitySlot.WBP_DBA_UI_AbilitySlot_C"));
    if (!TestNotNull(TEXT("真实技能槽类"), Class)) return false;
    TStrongObjectPtr<UGamePlatformSlotWidget> Slot(CreateWidget<UGamePlatformSlotWidget>(Instance.Get(), Class));
    if (!TestNotNull(TEXT("真实技能槽实例"), Slot.Get())) return false;
    FGamePlatformUISlotState State; State.SlotId = TEXT("SkillTest"); State.ContentId = TEXT("SpellTest"); State.bEnabled = false;
    Slot->ApplySlotState(State); Slot->ApplyOverlayTiming(123.f, 3.f);
    auto* Sweep = Cast<UImage>(Slot->GetWidgetFromName(TEXT("CooldownSweep")));
    if (!TestNotNull(TEXT("真实旋转遮罩Image"), Sweep)) return false;
    auto* Material = Sweep->GetDynamicMaterial(); if (!TestNotNull(TEXT("每实例动态材质"), Material)) return false;
    TestEqual(TEXT("事件设置不可重启的本地截止时刻"), Material->K2_GetScalarParameterValue(TEXT("CooldownEndsAt")), 123.f);
    TestEqual(TEXT("事件保留权威复制的总冷却"), Material->K2_GetScalarParameterValue(TEXT("CooldownDuration")), 3.f);
    TestEqual(TEXT("遮罩不拦截技能输入"), Sweep->GetVisibility(), ESlateVisibility::HitTestInvisible);
    Slot->ApplyOverlayTiming(0.f, 0.f);
    TestEqual(TEXT("终态事件立即清除遮罩"), Sweep->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("清理不会放宽冷却技能输入资格"), Slot->GetSlotState().bEnabled, false);
    return true;
}
#endif
