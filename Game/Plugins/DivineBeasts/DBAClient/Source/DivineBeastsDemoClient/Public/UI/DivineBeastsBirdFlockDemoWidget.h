// 神兽联盟鸟群演示UI桥接：视觉Widget树仍必须由Monolith真实生成；本类只连接命令和事件。
// 仅Development客户端/Editor；没有真实场景组件时明确显示错误，不调用历史ISM渲染器。
#pragma once

#include "CoreMinimal.h"
#include "UI/DivineBeastsDemoHubWidget.h"
#include "DivineBeastsBirdFlockDemoWidget.generated.h"

class UButton;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;
class UDivineBeastsDemoWorldSubsystem;

/**
 * UDivineBeastsBirdFlockDemoWidget（鸟群专题演示控件蓝图基类）。
 * 真实WBP命名控件由Monolith创建，C++只做Native事件绑定；没有Tick和正式后端调用。
 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsBirdFlockDemoWidget
    : public UDivineBeastsDemoHubWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;

    /** 每次真实命令结束才刷新，不使用逐帧UI轮询。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|BirdFlock")
    void RefreshBirdFlockStatus();

private:
    /** Optional使未交付的Monolith控件保留错误状态，不阻止项目编辑器启动。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UComboBoxString> SpeciesCombo;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UComboBoxString> FlightModeCombo;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> BirdCountInput;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> GlideBlendInput;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> SeparationInput;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> AlignmentInput;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> CohesionInput;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> ApplyBirdCountButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> ApplySpeciesButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> ApplyFlightModeButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> ApplyFlockingButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> ApplyGlideButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> StartBatchButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> StopBatchButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> ResetButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> BackButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> FeedbackText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> RealGpuCountText;

    UDivineBeastsDemoWorldSubsystem* GetBirdWorldDemo() const;
    static bool ReadFloat(const UEditableTextBox* Input,float& OutValue);
    static bool ReadInt(const UEditableTextBox* Input,int32& OutValue);
    UFUNCTION() void HandleApplyCount();
    UFUNCTION() void HandleApplySpecies();
    UFUNCTION() void HandleApplyFlightMode();
    UFUNCTION() void HandleApplyFlocking();
    UFUNCTION() void HandleApplyGlide();
    UFUNCTION() void HandleStartBatch();
    UFUNCTION() void HandleStopBatch();
    UFUNCTION() void HandleReset();
    UFUNCTION() void HandleBack();
};
