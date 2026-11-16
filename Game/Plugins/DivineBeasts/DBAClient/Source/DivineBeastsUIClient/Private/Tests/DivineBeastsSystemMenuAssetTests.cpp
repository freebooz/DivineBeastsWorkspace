// 神兽联盟第三层公共界面真实资产合同回归；由DivineBeastsUIClient在Editor/Client目标编译。
// 只读取Monolith已保存Widget类并Initialize临时实例，核对父类、命名控件、焦点与等高布局。
// 不激活页面、不挂视口、不创建ViewModel、不改设备设置、不点击注销/退出；临时实例由强引用保活到断言结束。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Definitions/GamePlatformSFXDefinition.h"
#include "Sound/SoundBase.h"
#include "Screens/DivineBeastsUIScreenCatalog.h"
#include "Screens/System/DivineBeastsDeviceSettingsScreen.h"
#include "Screens/System/DivineBeastsSystemMenuScreen.h"

namespace
{
/**
 * 读取目录指定的真实软类，不以临时WidgetTree或原生CDO替代保存资产。
 * 成功返回已Initialize但未激活的临时实例；任一前提失败均向实际测试记录错误并返回空强引用。
 * NativeParent必须是已批准C++页面父类；ScreenId必须来自现有项目目录，软路径不能指向未交付替身。
 */
TStrongObjectPtr<UGamePlatformUIScreen> LoadSavedSystemScreen(
    FAutomationTestBase& Test, FName ScreenId, UClass* NativeParent)
{
    const auto* Descriptor = FDivineBeastsUIScreenCatalog::Find(ScreenId);
    if (!Test.TestNotNull(TEXT("真实页面必须有目录合同"), Descriptor)) return {};
    UClass* SavedClass = LoadClass<UGamePlatformUIScreen>(nullptr, *Descriptor->WidgetClassPath);
    if (!Test.TestNotNull(TEXT("必须加载目录指向的已保存Widget蓝图生成类"), SavedClass)) return {};
    if (!Test.TestEqual(TEXT("已保存蓝图直接继承批准的原生页面父类"), SavedClass->GetSuperClass(), NativeParent)) return {};
    TStrongObjectPtr<UGamePlatformUIScreen> Screen(NewObject<UGamePlatformUIScreen>(GetTransientPackage(), SavedClass));
    if (!Test.TestNotNull(TEXT("真实类的临时Widget实例"), Screen.Get())) return {};
    if (!Test.TestTrue(TEXT("Initialize应复制真实WidgetTree并完成BindWidget"), Screen->Initialize())) return {};
    if (!Test.TestNotNull(TEXT("Initialize后实际控件树存在"), Screen->WidgetTree.Get())) return {};
    Test.TestFalse(TEXT("合同核验不能激活业务页"), Screen->IsActivated());
    Test.TestNull(TEXT("合同核验不创建业务ViewModel"), Screen->GetViewModel());

    // 使用平台公开运行入口设置目录默认焦点，不调用ActivateWidget或产生页面BeginPage。
    // 焦点由目录运行定义提供；不要求资产另存一份工具特有的初始焦点字段。
    Screen->InitializeScreen(ScreenId, nullptr, Descriptor->DefaultFocusWidgetName, Descriptor->InputMode, Descriptor->PausePolicy);
    UWidget* ExpectedFocus = Screen->GetWidgetFromName(Descriptor->DefaultFocusWidgetName);
    Test.TestNotNull(TEXT("目录默认焦点必须指向实际命名控件"), ExpectedFocus);
    Test.TestEqual(TEXT("原生运行Definition默认焦点返回目录实际控件"), Screen->GetDesiredFocusTarget(), ExpectedFocus);

    TArray<UWidget*> AuthoredWidgets;
    Screen->WidgetTree->GetAllWidgets(AuthoredWidgets);
    for (UWidget* AuthoredWidget : AuthoredWidgets)
    {
        // 固定字体/控件布局不能因插入ScaleBox重新缩放；遍历保存树，不靠控件名称猜测。
        Test.TestFalse(*FString::Printf(TEXT("固定布局不得包含ScaleBox：%s"), *AuthoredWidget->GetName()), AuthoredWidget->IsA<UScaleBox>());
    }
    return Screen;
}

/** 必需按钮的类型和键盘焦点合同；禁用状态不等同于焦点能力，测试不执行任何按钮命令。 */
UButton* VerifySavedFocusableButton(FAutomationTestBase& Test, UGamePlatformUIScreen& Screen, FName Name)
{
    auto* Button = Cast<UButton>(Screen.GetWidgetFromName(Name));
    if (Test.TestNotNull(*FString::Printf(TEXT("必需UButton：%s"), *Name.ToString()), Button))
        Test.TestTrue(*FString::Printf(TEXT("按钮允许键盘焦点：%s"), *Name.ToString()), Button->GetIsFocusable());
    return Button;
}

/** 必需下拉框由系统设置页一次命令读取；这里只验证真实类型/焦点，不选择选项或应用设置。 */
void VerifySavedFocusableCombo(FAutomationTestBase& Test, UGamePlatformUIScreen& Screen, FName Name)
{
    auto* Combo = Cast<UComboBoxString>(Screen.GetWidgetFromName(Name));
    if (Test.TestNotNull(*FString::Printf(TEXT("必需UComboBoxString：%s"), *Name.ToString()), Combo))
        Test.TestTrue(*FString::Printf(TEXT("下拉框允许键盘焦点：%s"), *Name.ToString()), Combo->IsFocusable());
}

/** 状态/错误正文必须是实际TextBlock；说明文本不要求交互焦点。 */
void VerifySavedTextBlock(FAutomationTestBase& Test, UGamePlatformUIScreen& Screen, FName Name)
{
    Test.TestNotNull(*FString::Printf(TEXT("必需UTextBlock：%s"), *Name.ToString()), Cast<UTextBlock>(Screen.GetWidgetFromName(Name)));
}

/**
 * Monolith已交付菜单按钮直接放在Canvas，读取其真实Slot尺寸，不假设存在SizeBox祖先。
 * 要求非拉伸锚点、关闭AutoSize和有限正尺寸，GetSize才表达固定宽高。单位为UMG设计像素。
 * 失败返回零尺寸并记录测试错误，不能将未知布局当作等高成功。
 */
FVector2D VerifySavedMenuButtonSize(FAutomationTestBase& Test, UButton& Button)
{
    auto* CanvasSlot = Cast<UCanvasPanelSlot>(Button.Slot);
    if (!Test.TestNotNull(*FString::Printf(TEXT("菜单按钮真实CanvasSlot：%s"), *Button.GetName()), CanvasSlot)) return FVector2D::ZeroVector;
    const auto Anchors = CanvasSlot->GetAnchors();
    Test.TestTrue(TEXT("按钮锚点不拉伸，Slot尺寸表示固定像素"), Anchors.Minimum.Equals(Anchors.Maximum));
    Test.TestFalse(TEXT("菜单按钮关闭AutoSize以保持固定高度"), CanvasSlot->GetAutoSize());
    const FVector2D SizePixels = CanvasSlot->GetSize();
    if (!Test.TestTrue(TEXT("固定按钮尺寸为有限正像素值"),
        FMath::IsFinite(SizePixels.X) && FMath::IsFinite(SizePixels.Y) && SizePixels.X > 0.0 && SizePixels.Y > 0.0)) return FVector2D::ZeroVector;
    return SizePixels;
}
}

/** Editor和Client各加载真实菜单；父类/按钮/焦点/等高/无缩放任一不符都必须失败，不能冒充视觉验收。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsSystemMenuAssetContractTest,
    "DivineBeasts.UI.SystemMenu.AssetContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsSystemMenuAssetContractTest::RunTest(const FString&)
{
    auto Screen = LoadSavedSystemScreen(*this, TEXT("UI.Screen.SystemMenu"), UDivineBeastsSystemMenuScreen::StaticClass());
    if (!Screen.IsValid()) return false;
    FVector2D ExpectedSizePixels = FVector2D::ZeroVector;
    for (const TCHAR* Name : {TEXT("ResumeButton"), TEXT("SettingsButton"), TEXT("LogoutButton"), TEXT("QuitButton")})
    {
        UButton* Button = VerifySavedFocusableButton(*this, *Screen.Get(), FName(Name));
        if (!Button) continue;
        const FVector2D SizePixels = VerifySavedMenuButtonSize(*this, *Button);
        if (SizePixels.Y <= 0.0) continue;
        TestTrue(TEXT("已交付菜单按钮保持280×70固定尺寸"), SizePixels.Equals(FVector2D(280.0, 70.0)));
        if (ExpectedSizePixels.Y <= 0.0) ExpectedSizePixels = SizePixels;
        else TestTrue(TEXT("四个菜单按钮保持同一固定宽高"), SizePixels.Equals(ExpectedSizePixels));
    }
    VerifySavedTextBlock(*this, *Screen.Get(), TEXT("ErrorText"));
    TestFalse(TEXT("检查结束仍未激活菜单"), Screen->IsActivated());
    return true;
}

/** 真实设置页只验证保存树与绑定合同；不会激活ViewModel、建立CoreTicker或改变实际显示设备设置。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDeviceSettingsAssetContractTest,
    "DivineBeasts.UI.Settings.AssetContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDeviceSettingsAssetContractTest::RunTest(const FString&)
{
    auto Screen = LoadSavedSystemScreen(*this, TEXT("UI.Screen.Settings"), UDivineBeastsDeviceSettingsScreen::StaticClass());
    if (!Screen.IsValid()) return false;
    for (const TCHAR* Name : {TEXT("ResolutionCombo"), TEXT("WindowModeCombo"), TEXT("QualityCombo")})
        VerifySavedFocusableCombo(*this, *Screen.Get(), FName(Name));
    for (const TCHAR* Name : {TEXT("ApplyButton"), TEXT("ConfirmButton"), TEXT("RevertButton"), TEXT("BackButton")})
        VerifySavedFocusableButton(*this, *Screen.Get(), FName(Name));
    for (const TCHAR* Name : {TEXT("StatusText"), TEXT("ErrorText")}) VerifySavedTextBlock(*this, *Screen.Get(), FName(Name));
    for (const TCHAR* Name : {TEXT("EffectsVolumeSlider"), TEXT("AmbientVolumeSlider"), TEXT("MusicVolumeSlider")})
    {
        auto* Slider = Cast<USlider>(Screen->GetWidgetFromName(FName(Name)));
        if (!TestNotNull(TEXT("真实音量滑杆"), Slider)) continue;
        TestEqual(TEXT("滑杆下限0%"), Slider->GetMinValue(), 0.0f);
        TestEqual(TEXT("滑杆上限100%"), Slider->GetMaxValue(), 1.0f);
        TestEqual(TEXT("键盘手柄步进1%"), Slider->GetStepSize(), 0.01f);
        TestTrue(TEXT("音量滑杆支持键盘手柄焦点"), Slider->IsFocusable);
    }
    for (const TCHAR* Name : {TEXT("EffectsVolumeText"), TEXT("AmbientVolumeText"), TEXT("MusicVolumeText"), TEXT("AudioHelpText")})
        VerifySavedTextBlock(*this, *Screen.Get(), FName(Name));
    TestFalse(TEXT("检查结束仍未激活系统设置页"), Screen->IsActivated());
    TestNull(TEXT("只读资产检查没有业务VM或显示设置命令"), Screen->GetViewModel());
    return true;
}

/** 读取真实保存Definition及声音，验证音乐/环境分类和循环静音恢复合同；不以路径推断替代资产值。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsAudioGroupsAssetContractTest,
    "DivineBeasts.UI.Settings.AudioGroupsAssetContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsAudioGroupsAssetContractTest::RunTest(const FString&)
{
    struct FCase { const TCHAR* Path; EGamePlatformSFXSoundGroup Group; };
    const FCase Cases[] = {
        {TEXT("/DBASFXPack_Core/Audio/Music/Definitions/DA_DBA_SFX_Music_Login"), EGamePlatformSFXSoundGroup::Music},
        {TEXT("/DBASFXPack_Core/Audio/Music/Definitions/DA_DBA_SFX_Music_Village"), EGamePlatformSFXSoundGroup::Music},
        {TEXT("/DBASFXPack_Core/Audio/Ambience/Definitions/DA_DBA_SFX_Ambience_Forest"), EGamePlatformSFXSoundGroup::Ambient},
        {TEXT("/DBASFXPack_Core/Audio/Library/Ambience/Definitions/DA_DBA_SFX_Ambience_WaterStreamA"), EGamePlatformSFXSoundGroup::Ambient},
        {TEXT("/DBASFXPack_Core/Audio/Library/Ambience/Definitions/DA_DBA_SFX_Ambience_WaterStreamB"), EGamePlatformSFXSoundGroup::Ambient},
        {TEXT("/DBASFXPack_Core/Audio/Library/Ambience/Definitions/DA_DBA_SFX_Ambience_Birds"), EGamePlatformSFXSoundGroup::Ambient},
        {TEXT("/DBASFXPack_Core/Weather/Definitions/DA_DBA_SFX_Weather_Rain"), EGamePlatformSFXSoundGroup::Ambient},
        {TEXT("/DBASFXPack_Core/Weather/Definitions/DA_DBA_SFX_Weather_Snow"), EGamePlatformSFXSoundGroup::Ambient}
    };
    for (const auto& Case : Cases)
    {
        auto* Definition = LoadObject<UGamePlatformSFXDefinition>(nullptr, Case.Path);
        if (!TestNotNull(*FString::Printf(TEXT("保存的音频Definition：%s"), Case.Path), Definition)) continue;
        TestEqual(TEXT("原生Definition明确用户混音分组"), Definition->GetSoundGroup(), Case.Group);
        auto* Sound = LoadObject<USoundBase>(nullptr, *Definition->GetSound().ToSoftObjectPath().ToString());
        if (!TestNotNull(TEXT("Definition实际声音"), Sound)) continue;
        if (Sound->IsLooping())
            TestEqual(TEXT("持续声音静音时仍由原生音频保持播放可恢复"), Sound->VirtualizationMode, EVirtualizationMode::PlayWhenSilent);
    }
    return true;
}
#endif
