// 平台输入合同回归：真实SceneViewport验证鼠标按下能否进入游戏，不连接后端或捕获桌面。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Input/GamePlatformUIInputPolicy.h"
#include "CommonInputModeTypes.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "InputKeyEventArgs.h"
#include "ViewportClient.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUIInputPolicyTest,
    "GamePlatform.UI.Input.PolicyModes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUIInputPolicyTest::RunTest(const FString& Parameters)
{
    const FUIInputConfig GameOnly =
        UGamePlatformUIInputPolicy::BuildInputConfig(EGamePlatformUIInputMode::GameOnly);
    const FUIInputConfig UIOnly =
        UGamePlatformUIInputPolicy::BuildInputConfig(EGamePlatformUIInputMode::UIOnly);
    const FUIInputConfig Both =
        UGamePlatformUIInputPolicy::BuildInputConfig(EGamePlatformUIInputMode::GameAndUI);

    TestEqual(TEXT("GameOnly"), GameOnly.GetInputMode(), ECommonInputMode::Game);
    TestEqual(TEXT("UIOnly"), UIOnly.GetInputMode(), ECommonInputMode::Menu);
    TestEqual(TEXT("GameAndUI"), Both.GetInputMode(), ECommonInputMode::All);
    TestEqual(TEXT("GameAndUI必须允许世界鼠标首次按下"), Both.GetMouseCaptureMode(), EMouseCaptureMode::CaptureDuringMouseDown);
    TestTrue(TEXT("UIOnly忽略移动"), UIOnly.bIgnoreMoveInput);
    TestTrue(TEXT("UIOnly忽略视角"), UIOnly.bIgnoreLookInput);
    TestFalse(TEXT("无World不能暂停"), UGamePlatformUIInputPolicy::CanPauseWorld(
        nullptr,
        EGamePlatformUIPausePolicy::StandaloneOnly));
    return true;
}

#if WITH_EDITOR
namespace
{
// 仅记录真实引擎分发，测试替身不冒充游戏控制器、Gameplay准入或玩家账号。
class FMouseDispatchClient final : public FViewportClient
{
public:
    EMouseCaptureMode CaptureMode = EMouseCaptureMode::NoCapture;
    int32 MousePresses = 0;
    virtual EMouseCaptureMode GetMouseCaptureMode() const override { return CaptureMode; }
    virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override
    {
        if (EventArgs.Event == IE_Pressed && (EventArgs.Key == EKeys::LeftMouseButton || EventArgs.Key == EKeys::RightMouseButton)) ++MousePresses;
        return true;
    }
};
// 仅输入尺寸夹具，不分配RHI或创建OS窗口；PIE标记选择真实游戏视口分发分支。
class FMouseDispatchSceneViewport final : public FSceneViewport
{
public:
    explicit FMouseDispatchSceneViewport(const TSharedPtr<SViewport>& Widget) : FSceneViewport(Widget)
    { SizeX = 640; SizeY = 480; SetPlayInEditorViewport(true); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformGameplayMousePressTest,
    "GamePlatform.UI.Input.GameplayMousePress", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformGameplayMousePressTest::RunTest(const FString&)
{
    const TSharedPtr<FMouseDispatchClient> Client = MakeShared<FMouseDispatchClient>();
    const TSharedRef<SViewport> Surface = SNew(SViewport);
    const TSharedRef<FMouseDispatchSceneViewport> Viewport = MakeShared<FMouseDispatchSceneViewport>(Surface);
    Viewport->SetViewportClient(TStrongPtrVariant<FViewportClient>(StaticCastSharedPtr<FViewportClient>(Client)));
    Surface->SetViewportInterface(Viewport);
    TestEqual(TEXT("真实分发前非零视口尺寸"), Viewport->GetSizeXY(), FIntPoint(640, 480));
    TestTrue(TEXT("采用游戏/PIE视口分发，不能走编辑器无条件输入分支"), Viewport->IsPlayInEditorViewport());
    TestFalse(TEXT("首次按下前尚未捕获鼠标"), Viewport->HasMouseCapture());
    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(640, 480), FSlateLayoutTransform());
    for (const FKey Key : {EKeys::LeftMouseButton, EKeys::RightMouseButton})
    {
        const TSet<FKey> Pressed {Key};
        const FPointerEvent Press(0, FVector2D(32, 32), FVector2D(32, 32), Pressed, Key, 0.f, FModifierKeysState());
        Client->CaptureMode = EMouseCaptureMode::NoCapture; Client->MousePresses = 0;
        // 不应用FReply到Slate，避免操作真实桌面；只调用引擎输入分发。
        Viewport->OnMouseButtonDown(Geometry, Press);
        TestEqual(TEXT("旧NoCapture实际丢失游戏鼠标按下"), Client->MousePresses, 0);
        Client->CaptureMode = UGamePlatformUIInputPolicy::BuildInputConfig(EGamePlatformUIInputMode::GameAndUI).GetMouseCaptureMode();
        Viewport->OnMouseButtonDown(Geometry, Press);
        TestEqual(TEXT("GameAndUI左右键首次按下各分发一次"), Client->MousePresses, 1);
        const FPointerEvent Release(0, FVector2D(32, 32), FVector2D(32, 32), TSet<FKey>(), Key, 0.f, FModifierKeysState());
        Viewport->OnMouseButtonUp(Geometry, Release);
    }
    return true;
}
#endif // 真实SceneViewport夹具仅编译进Editor，不进入发行客户端。

#endif
