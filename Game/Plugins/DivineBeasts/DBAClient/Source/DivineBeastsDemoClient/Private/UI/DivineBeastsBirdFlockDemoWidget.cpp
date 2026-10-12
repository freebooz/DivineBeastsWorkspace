// 鸟群UI只负责把Monolith控件的真实用户操作转交正式世界演示子系统。
// 原生绑定事件驱动更新结果，不在Widget中计算或同步鸟群权威及GPU粒子。
#include "UI/DivineBeastsBirdFlockDemoWidget.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Misc/LexFromString.h"

void UDivineBeastsBirdFlockDemoWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (SpeciesCombo)
    {
        for (const TCHAR* Species : {TEXT("Seagull"), TEXT("Swallow"), TEXT("Eagle")})
            SpeciesCombo->AddOption(Species);
        SpeciesCombo->SetSelectedOption(TEXT("Swallow"));
    }
    if (FlightModeCombo)
    {
        for (const TCHAR* Mode : {TEXT("Orbit"),TEXT("Traverse"),TEXT("FigureEight"),
                                   TEXT("SplineOneWay"),TEXT("SplineLoop"),TEXT("SplinePingPong")})
            FlightModeCombo->AddOption(Mode);
        FlightModeCombo->SetSelectedOption(TEXT("Orbit"));
    }
    if (ApplyBirdCountButton) ApplyBirdCountButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleApplyCount);
    if (ApplySpeciesButton) ApplySpeciesButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleApplySpecies);
    if (ApplyFlightModeButton) ApplyFlightModeButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleApplyFlightMode);
    if (ApplyFlockingButton) ApplyFlockingButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleApplyFlocking);
    if (ApplyGlideButton) ApplyGlideButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleApplyGlide);
    if (StartBatchButton) StartBatchButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleStartBatch);
    if (StopBatchButton) StopBatchButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleStopBatch);
    if (ResetButton) ResetButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleReset);
    if (BackButton) BackButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleBack);
    RefreshBirdFlockStatus();
}

UDivineBeastsDemoWorldSubsystem* UDivineBeastsBirdFlockDemoWidget::GetBirdWorldDemo() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UDivineBeastsDemoWorldSubsystem>() : nullptr;
}

bool UDivineBeastsBirdFlockDemoWidget::ReadInt(const UEditableTextBox* Input,int32& OutValue)
{
    return Input && LexTryParseString(OutValue,*Input->GetText().ToString());
}

bool UDivineBeastsBirdFlockDemoWidget::ReadFloat(const UEditableTextBox* Input,float& OutValue)
{
    return Input && LexTryParseString(OutValue,*Input->GetText().ToString()) &&
        FMath::IsFinite(OutValue);
}

void UDivineBeastsBirdFlockDemoWidget::RefreshBirdFlockStatus()
{
    auto* WorldDemo = GetBirdWorldDemo();
    if (FeedbackText)
        FeedbackText->SetText(WorldDemo ? WorldDemo->GetLastFeedback() :
            FText::FromString(TEXT("当前世界没有正式鸟群演示服务。")));
    if (RealGpuCountText)
    {
        const int32 Actual = WorldDemo ? WorldDemo->GetActualBirdCount() : INDEX_NONE;
        RealGpuCountText->SetText(Actual >= 0 ?
            FText::FromString(FString::Printf(TEXT("真实GPU鸟数：%d"),Actual)) :
            FText::FromString(TEXT("真实GPU鸟数：待GPU读回，不能用请求数替代")));
    }
}

void UDivineBeastsBirdFlockDemoWidget::HandleApplyCount()
{
    int32 Count=0;
    if (auto* Demo=GetBirdWorldDemo(); Demo && ReadInt(BirdCountInput,Count))
        Demo->ApplyBirdCount(Count);
    RefreshBirdFlockStatus();
}

void UDivineBeastsBirdFlockDemoWidget::HandleApplySpecies()
{
    if (auto* Demo=GetBirdWorldDemo(); Demo && SpeciesCombo)
        Demo->ApplyBirdSpecies(FName(*SpeciesCombo->GetSelectedOption()));
    RefreshBirdFlockStatus();
}

void UDivineBeastsBirdFlockDemoWidget::HandleApplyFlightMode()
{
    if (auto* Demo=GetBirdWorldDemo(); Demo && FlightModeCombo)
    {
        const int32 Index=FlightModeCombo->FindOptionIndex(FlightModeCombo->GetSelectedOption());
        if (Index>=0 && Index<=5)
            Demo->ApplyBirdFlightMode(static_cast<EDivineBeastsBirdFlockFlightMode>(Index));
    }
    RefreshBirdFlockStatus();
}

void UDivineBeastsBirdFlockDemoWidget::HandleApplyFlocking()
{
    float Separation=0,Alignment=0,Cohesion=0;
    if (auto* Demo=GetBirdWorldDemo(); Demo && ReadFloat(SeparationInput,Separation) &&
        ReadFloat(AlignmentInput,Alignment) && ReadFloat(CohesionInput,Cohesion))
        Demo->ApplyBirdFlocking(Separation,Alignment,Cohesion);
    RefreshBirdFlockStatus();
}

void UDivineBeastsBirdFlockDemoWidget::HandleApplyGlide()
{
    float Glide=0;
    if (auto* Demo=GetBirdWorldDemo(); Demo && ReadFloat(GlideBlendInput,Glide))
        Demo->ApplyBirdGlide(Glide);
    RefreshBirdFlockStatus();
}

void UDivineBeastsBirdFlockDemoWidget::HandleStartBatch()
{
    if (auto* Demo=GetBirdWorldDemo()) Demo->StartBirdBatches();
    RefreshBirdFlockStatus();
}

void UDivineBeastsBirdFlockDemoWidget::HandleStopBatch()
{
    if (auto* Demo=GetBirdWorldDemo()) Demo->StopBirdBatches();
    RefreshBirdFlockStatus();
}

void UDivineBeastsBirdFlockDemoWidget::HandleReset()
{
    if (auto* Demo=GetBirdWorldDemo()) Demo->ResetDemoChanges();
    RefreshBirdFlockStatus();
}

void UDivineBeastsBirdFlockDemoWidget::HandleBack()
{
    RequestReturnToHub();
}
