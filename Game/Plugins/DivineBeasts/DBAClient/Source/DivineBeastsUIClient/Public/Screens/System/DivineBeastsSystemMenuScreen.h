// 第三层游戏菜单命令适配；视觉树唯一归Monolith资产，平台层栈拥有焦点/输入，页面不暂停世界。
#pragma once
#include "Screens/DivineBeastsUIScreen.h"
#include "DivineBeastsSystemMenuScreen.generated.h"
class UButton;
class UTextBlock;

/** 竖排游戏菜单：返回、真实系统设置、既有注销命令、正常退出；无Tick/HTTP/业务状态写入。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsSystemMenuScreen : public UDivineBeastsUIScreen
{
    GENERATED_BODY()
public:
    /** 只指定项目System语义，不加载视觉资源或连接业务。 */
    UDivineBeastsSystemMenuScreen();
protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) override;
    /** 四个稳定命名按钮由Monolith生成；等高、藤蔓/叶片/碧绿远古样式由资产负责。 */
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ResumeButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SettingsButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> LogoutButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> QuitButton;
    /** 资源/命令失败反馈；不显示内部软路径或敏感认证字段。 */
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ErrorText;
private:
    void RefreshCommands();
    UFUNCTION() void HandleResume();
    UFUNCTION() void HandleSettings();
    UFUNCTION() void HandleLogout();
    UFUNCTION() void HandleQuit();
};
