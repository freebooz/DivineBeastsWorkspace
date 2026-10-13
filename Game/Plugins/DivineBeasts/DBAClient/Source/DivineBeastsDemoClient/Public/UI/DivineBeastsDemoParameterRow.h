// SSLM案例参数值编辑桥；页面拥有开发输入副本，显式提交前不改变任何场景对象。
#pragma once
#include "Components/GamePlatformComponentWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Contracts/DivineBeastsDemoCatalogTypes.h"
#include "DivineBeastsDemoParameterRow.generated.h"

/** 虚拟化参数项：有类型的开发输入，解析失败保存错误状态，不发送控制台或脚本。 */
UCLASS(BlueprintType)
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoParameterItem : public UObject
{
    GENERATED_BODY()
public:
    /** 该参数唯一身份、单位、范围、默认值及应用阶段的只读合同副本。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FDivineBeastsDemoParameterDefinition Definition;
    /** 当前合法编辑值，类型由Definition约束；解析失败不覆盖，显式提交前无世界副作用。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FDivineBeastsDemoParameterValue Value;
    /** 编辑内容是否可提交；默认值已由目录预检验证，任何解析失败设为false。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") bool bValidInput = true;
    /** 解析严格类型并校验范围/只读；失败不覆盖上一合法值，bValidInput为false。 */
    bool SetTextValue(const FString& Text);
    /** 仅boolean及可写execute参数允许变更；其余明确拒绝。 */
    bool SetBooleanValue(bool bValue);
};

/** Monolith参数条目父类；只绑定输入和说明，实际视觉布局统一由引擎资产持有。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoParameterRow : public UGamePlatformComponentWidget, public IUserObjectListEntry
{
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnListItemObjectSet(UObject* Object) override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> ParameterLabelText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> ParameterErrorText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UEditableTextBox> ParameterValueInput;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UCheckBox> ParameterBooleanInput;
private:
    /** 条目重用/初始化设置文本时抑制重入。 */
    bool bSynchronizing = false;
    TWeakObjectPtr<UDivineBeastsDemoParameterItem> Item;
    UFUNCTION() void OnTextChanged(const FText& Text);
    UFUNCTION() void OnBooleanChanged(bool bChecked);
};
