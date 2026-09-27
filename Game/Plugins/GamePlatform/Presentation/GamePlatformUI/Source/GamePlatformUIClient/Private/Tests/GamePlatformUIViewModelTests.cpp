#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ViewModels/GamePlatformViewModelBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUIViewModelRevisionTest,
    "GamePlatform.UI.ViewModel.RevisionAndGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUIViewModelRevisionTest::RunTest(const FString& Parameters)
{
    UGamePlatformViewModelBase* ViewModel = NewObject<UGamePlatformViewModelBase>();
    TestFalse(TEXT("初始页面未激活"), ViewModel->IsPageActive());

    ViewModel->BeginPage();
    const int32 Revision = ViewModel->GetRevision();
    const int32 Generation = ViewModel->GetPageGeneration();
    TestTrue(TEXT("BeginPage 后激活"), ViewModel->IsPageActive());
    TestTrue(TEXT("当前回调有效"), ViewModel->IsCallbackCurrent(Revision, Generation));

    ViewModel->MarkStateChanged();
    TestFalse(TEXT("旧 Revision 必须失效"), ViewModel->IsCallbackCurrent(Revision, Generation));

    ViewModel->EndPage();
    TestFalse(TEXT("EndPage 后旧回调失效"), ViewModel->IsCallbackCurrent(
        ViewModel->GetRevision(),
        Generation));
    return true;
}

#endif
