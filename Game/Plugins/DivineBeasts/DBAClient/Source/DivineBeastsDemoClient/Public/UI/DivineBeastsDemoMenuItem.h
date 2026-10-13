// SSLM演示ListView值对象与条目桥接；本地Widget拥有，视觉树由Monolith制作。
#pragma once
#include "Components/GamePlatformComponentWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "DivineBeastsDemoMenuItem.generated.h"

/** 仅承载ListView选择身份和只读显示文字，不能持有业务对象或执行命令。 */
UCLASS(BlueprintType)
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoMenuItem : public UObject
{
    GENERATED_BODY()
public:
    /** 身份按列表领域解释，选择事件不从中文标题反推。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FName Identity;
    /** 中文名称或稳定插件名。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FText Label;
    /** 禁用/四维状态/缺口提示，不能单凭颜色表达。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|Demo") FText Status;
};

/** Monolith条目蓝图父类；只将值对象绑定到命名文字控件，不生成视觉布局。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoMenuEntryWidget : public UGamePlatformComponentWidget, public IUserObjectListEntry
{
    GENERATED_BODY()
protected:
    /** 虚拟化条目重用时替换显示，旧数据不会保存在业务状态。 */
    virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> LabelText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> StatusText;
};
