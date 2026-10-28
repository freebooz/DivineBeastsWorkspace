// 只测真实主题服务的空快照/来源与修订门禁；不创建第二份真实GI子系统。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Styling/GamePlatformUIThemeService.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIThemeSnapshotEmptyTest,"GamePlatform.UI.Theme.SnapshotEmptyAndConflict",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGamePlatformUIThemeSnapshotEmptyTest::RunTest(const FString&)
{
    if(!TestNotNull(TEXT("需要真实Engine作为独立值夹具Outer"),GEngine))return false;
    TStrongObjectPtr<ULocalPlayer> P(NewObject<ULocalPlayer>(GEngine));
    TStrongObjectPtr<UGamePlatformUIManagerSubsystem> Owner(NewObject<UGamePlatformUIManagerSubsystem>(P.Get()));
    TStrongObjectPtr<UGamePlatformUIThemeService> Service(NewObject<UGamePlatformUIThemeService>(Owner.Get()));
    // 没有真实GI、不初始化SubsystemCollection；本测试不请求任何Data或构造屏幕。
    Service->Initialize(Owner.Get());const auto Empty=Service->GetSnapshot();FGuid Request;
    TestTrue(TEXT("空主题也具备LP快照身份"),Empty.ScopeId.IsValid());TestFalse(TEXT("明确空主题"),Empty.ThemeId.IsValid());
    auto Other=Empty;Other.ScopeId=FGuid::NewGuid();TestFalse(TEXT("不同LP快照拒绝"),Service->RestoreSnapshot(Other,Empty.Revision,Request).IsSuccess());
    TestFalse(TEXT("外部新修订不被旧恢复覆盖"),Service->RestoreSnapshot(Empty,Empty.Revision+1,Request).IsSuccess());
    TestTrue(TEXT("同作用域空状态恢复走真实原子清空"),Service->RestoreSnapshot(Empty,Empty.Revision,Request).IsSuccess());
    TestFalse(TEXT("同步空恢复不伪造异步请求"),Request.IsValid());
    TestEqual(TEXT("状态修订单调递增"),Service->GetSnapshot().Revision,Empty.Revision+1);
    TestFalse(TEXT("旧修订再次恢复拒绝"),Service->RestoreSnapshot(Empty,Empty.Revision,Request).IsSuccess());
    Service->Shutdown();TestFalse(TEXT("关闭后的原快照不能复活主题"),Service->RestoreSnapshot(Empty,Empty.Revision+1,Request).IsSuccess());
    return true;
}
#endif
