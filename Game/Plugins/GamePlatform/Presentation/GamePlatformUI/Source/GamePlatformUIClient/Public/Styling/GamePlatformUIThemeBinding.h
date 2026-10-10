// 平台用户控件的主题兼容绑定；不创建Widget、不接管点击/焦点/业务可用性。
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Styling/GamePlatformUIThemeTypes.h"
#include "GamePlatformUIThemeBinding.generated.h"

class UUserWidget;
class UGamePlatformUIManagerSubsystem;

/** 一个界面实例拥有一个绑定对象；页面隐藏时保留已经应用的类引用，重新激活再刷新。 */
UCLASS()
class GAMEPLATFORMUICLIENT_API UGamePlatformUIThemeBinding final : public UObject
{
    GENERATED_BODY()
public:
    /** 仅游戏线程；无所属LocalPlayer返回false，绑定只属于传入控件自己的WidgetTree。 */
    bool Initialize(UUserWidget* Widget, const TArray<FGamePlatformUIWidgetStyleBinding>& InBindings);
    /** 注销事件但保留已有外观的强引用；页面实例真正销毁时才释放。 */
    void Unbind();
    /** 先验证全部必需绑定再应用；失败保持该页面旧外观并返回可诊断原因。 */
    bool Refresh();
    FText GetLastError() const { return LastError; }
    virtual void BeginDestroy() override;
private:
    UFUNCTION() void HandleThemeChanged(int64 Revision);
    TWeakObjectPtr<UUserWidget> OwnerWidget;
    TWeakObjectPtr<UGamePlatformUIManagerSubsystem> Manager;
    TArray<FGamePlatformUIWidgetStyleBinding> Bindings;
    /** 类CDO及其画刷/字体硬引用使已隐藏页面仍持有实际使用中的旧皮肤资源。 */
    UPROPERTY(Transient) TArray<TObjectPtr<UClass>> AppliedStyleClasses;
    FText LastError;
    bool bApplying = false;
};
